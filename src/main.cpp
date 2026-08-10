// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/OutgoingTelemetry.hpp"
#include "botix/config/Registry.hpp"
#include "botix/transport/Address.hpp"

#include "botix/system/BehaviorSystem.hpp"
#include "botix/system/ConfigSystem.hpp"
#include "botix/system/ConsoleSystem.hpp"
#include "botix/system/HardwareSystem.hpp"
#include "botix/system/NetworkSystem.hpp"
#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TelemetrySystem.hpp"
#include "botix/system/TransportSystem.hpp"

void kf::main(kf::Init &init) {
    init.logger.debug("Starting");

    // system instances

    static botix::system::ConfigSystem system_config{};

    static botix::system::HardwareSystem system_hardware{{
        .config = system_config.device,
    }};

    static botix::system::TelemetrySystem system_telemetry{{
        .config = system_config.device,
    }};

    static botix::system::NetworkSystem system_network{{
        .config = system_config.user.network,
        .service_port = system_config.user.transport_registry.wifi_udp.local_port,
    }};

    static botix::system::TransportSystem system_transport{{
        .config = system_config.user.transport_registry,
    }};

    static botix::system::ProtocolSystem system_protocol{{
        .config = system_config.device,
        .transport_link = system_transport.link,
        .outgoing_telemetry = system_telemetry.outgoing,
    }};

    static botix::system::BehaviorSystem system_behavior{{
        .config = system_config.device,
        .periphery = system_hardware.periphery,
        .incoming_telemetry = system_telemetry.incoming,
    }};

    // system initialization

    // config

    system_config.init();

    static botix::config::Registry config_registry{
        system_config.device,
        system_config.user,
    };

    // hardware

    if (not system_hardware.init()) {
        init.logger.error("Hardware init failed");
    }

    // network: the station must exist before the UDP transport can bind

    system_network.init();

    // telemetry

    system_telemetry.init();

    system_telemetry.outgoing.wheel_distance.callback([]() -> botix::OutgoingTelemetry::WheelDistance {
        return {
            .left_mm = system_hardware.periphery.wheel_odometry_encoder_left.positionUnits(),
            .right_mm = system_hardware.periphery.wheel_odometry_encoder_right.positionUnits(),
        };
    });

    // transport

    system_transport.init(system_config.user.init_transport_kind);

    system_transport.onReceive([](auto const &context) -> void {
        system_protocol.link.receive({
            .transport = context,
            .incoming_telemetry = system_telemetry.incoming,
            .timestamp = kf::rtos::Clock::now(),
        });
    });

    system_transport.onReceiveForeign([&init](auto const &context) -> void {
        // Traffic from anything that is not the peer is ordinary background noise
        // on a shared network. Logging it per packet floods the UART, garbles the
        // console sharing that line, and buries whatever the real problem is.
        if (system_transport.link.connected()) {
            return;
        }

        init.logger.debug("Found device: {}", context.address);

        if (not system_transport.link.connect(context.address)) {
            init.logger.error("connect failed: {}", context.address);
        }
    });

    // protocol

    system_protocol.init(system_config.user.init_protocol_kind);

    system_protocol.onRawFallback([&init](botix::transport::Address const &address, auto buffer) -> void {
        (void) address;
        init.logger.debug("raw (fallback): got {} bytes", buffer.length());
    });

    system_protocol.onMavlinkFallback([&init](botix::transport::Address const &address, auto const &message) -> void {
        (void) address;
        init.logger.debug("mavlink (fallback): msg id: {}, seq: {}", message.msgid, message.seq);
    });

    // console

    static botix::service::ConsoleService::Config console_config{};
    console_config.reset();

    static botix::system::ConsoleSystem system_console{{
        .config = console_config,
        .arena = init.arena,
        .registry = config_registry,
        .device_config_service = system_config.device_service,
        .user_config_service = system_config.user_service,
        .network = system_network.service,
        .transport = system_transport,
        .protocol = system_protocol,
        .incoming_telemetry = system_telemetry.incoming,
        .outgoing_telemetry = system_telemetry.outgoing,
        .max_control_input_age_ms = system_config.device.mixer_service.max_control_input_age_ms,
    }};

    if (not system_console.init()) {
        init.logger.error("Console init failed");
    }

    // Serial console: bytes come from the UART, output goes straight back to it
    auto maybe_serial_channel = system_console.addChannel([&init](kf::StringView line) -> void {
        (void) init.io.writeBuffer({
            reinterpret_cast<kf::u8 const *>(line.data()),
            line.length(),
        });
        init.io.flush();
    });

    // Remote console: bytes arrive over MAVLink SERIAL_CONTROL and replies go back the same way
    auto maybe_mavlink_channel = system_console.addChannel([](kf::StringView line) -> void {
        (void) system_protocol.sendSerialControl(line);
    });

    if (maybe_mavlink_channel.isSome()) {
        static auto &mavlink_channel = maybe_mavlink_channel.unwrap();

        system_protocol.onSerialControl([](kf::StringView text) -> void {
            mavlink_channel.feed(text);
        });
    } else {
        init.logger.error("MAVLink console channel unavailable");
    }

    // behavior

    system_behavior.init();

    init.logger.info("Ready");

    // loop

    auto serial_read_buffer = init.arena.allocate(64);

    while (true) {
        constexpr auto loop_period{1000 / 100};

        auto const now = rtos::Clock::now();

        system_telemetry.poll(now);
        system_network.poll(now);
        system_transport.poll(now);
        system_protocol.poll(now);
        system_behavior.poll(now);
        system_config.poll(now);
        system_hardware.poll(now);

        if (maybe_serial_channel.isSome() and init.io.availableForRead() > 0) {
            if (auto const read = init.io.readBuffer(serial_read_buffer); read.isOk()) {
                maybe_serial_channel.unwrap().feed({
                    reinterpret_cast<char const *>(read.ok().data()),
                    read.ok().length(),
                });
            }
        }

        system_console.poll(now);

        rtos::Task::sleep(loop_period);
    }
}
