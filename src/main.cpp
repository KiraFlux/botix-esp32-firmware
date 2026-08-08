// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/RootConfig.hpp"

#include "botix/OutgoingTelemetry.hpp"
#include "botix/protocol/Kind.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/system/BehaviorSystem.hpp"
#include "botix/system/HardwareSystem.hpp"
#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TelemetrySystem.hpp"
#include "botix/system/TransportSystem.hpp"

static botix::RootConfig root_config{};

void kf::main(kf::Init &init) {
    init.logger.debug("Starting");

    root_config.reset();// set to defaults

    // system instances

    static botix::system::HardwareSystem system_hardware{{
        .config = root_config,
    }};

    static botix::system::TelemetrySystem system_telemetry{{
        .config = root_config,
    }};

    static botix::system::TransportSystem system_transport{{
        // TODO: check for deps
    }};

    static botix::system::ProtocolSystem system_protocol{{
        .config = root_config,
        .transport_link = system_transport.link,
        .outgoing_telemetry = system_telemetry.outgoing,
    }};

    static botix::system::BehaviorSystem system_behavior{{
        .config = root_config,
        .periphery = system_hardware.periphery,
        .incoming_telemetry = system_telemetry.incoming,
    }};

    // system initialization

    // hardware

    system_hardware.init();

    // telemetry

    system_telemetry.init();

    system_telemetry.outgoing.wheel_distance.callback([]() -> botix::OutgoingTelemetry::WheelDistance {
        return {
            .left_mm = system_hardware.periphery.wheel_odometry_encoder_left.positionUnits(),
            .right_mm = system_hardware.periphery.wheel_odometry_encoder_right.positionUnits(),
        };
    });

    // transport

    system_transport.init(botix::transport::Kind::Espnow);

    system_transport.onReceive([](auto const &context) -> void {
        system_protocol.link.receive({
            .transport = context,
            .incoming_telemetry = system_telemetry.incoming,
            .timestamp = kf::rtos::Clock::now(),
        });
    });

    system_transport.onReceiveForeign([&init](auto const &context) -> void {
        init.logger.debug("Found device");

        if (system_transport.link.connected()) {
            init.logger.error("connect denied (already connected)");
            return;
        }

        if (not system_transport.link.connect(context.address)) {
            init.logger.error("system_transport.link.connect failed");
        }
    });

    // protocol

    system_protocol.init(botix::protocol::Kind::Mavlink);

    system_protocol.onRawFallback([&init](botix::transport::Address const &address, auto buffer) -> void {
        (void) address;
        init.logger.debug("raw (fallback): got {} bytes", buffer.length());
    });

    system_protocol.onMavlinkFallback([&init](botix::transport::Address const &address, auto const &message) -> void {
        (void) address;
        init.logger.debug("mavlink (fallback): msg id: {}, seq: {}", message.msgid, message.seq);
    });

    // behavior

    system_behavior.init();

    init.logger.info("Ready");

    // loop

    auto const on_input_char = [&init](char c) -> void {
        switch (c) {
            case 'o': {
                init.logger.debug(
                    "Encoders: \tL: {} \t R: {}",
                    system_telemetry.outgoing.wheel_distance.value().left_mm,
                    system_telemetry.outgoing.wheel_distance.value().right_mm);
                return;
            }

            case 'r': {
                system_protocol.link.set(system_protocol.get(botix::protocol::Kind::Raw));
                init.logger.debug("protocol: Raw");
                return;
            }

            case 'm': {
                system_protocol.link.set(system_protocol.get(botix::protocol::Kind::Mavlink));
                init.logger.debug("protocol: Mavlink");
                return;
            }
        }
    };

    while (true) {
        constexpr auto loop_period{1000 / 100};

        auto const now = rtos::Clock::now();

        system_telemetry.poll(now);
        system_transport.poll(now);
        system_protocol.poll(now);
        system_behavior.poll(now);
        system_hardware.poll(now);

        {
            while (init.io.availableForRead() > 0) {
                if (auto const read = init.io.readPacket<char>(); read.isOk()) {
                    on_input_char(read.ok());
                }
            }
        }

        rtos::Task::sleep(loop_period);
    }
}
