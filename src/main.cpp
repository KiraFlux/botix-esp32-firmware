// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/OutgoingTelemetry.hpp"
#include "botix/Periphery.hpp"
#include "botix/cli/Console.hpp"
#include "botix/protocol/Kind.hpp"
#include "botix/transport/Address.hpp"

#include "botix/system/BehaviorSystem.hpp"
#include "botix/system/ConfigSystem.hpp"
#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TelemetrySystem.hpp"
#include "botix/system/TransportSystem.hpp"

void kf::main(kf::Init &init) {
    init.logger.debug("Starting");

    // system instances

    static botix::system::ConfigSystem system_config{};

    static botix::Periphery periphery{system_config.device.periphery};

    static botix::cli::Console console{init.arena, system_config.device.cli};

    auto maybe_main_channel = console.addChannel(init.arena, {.echo = true});
    auto maybe_other_channel = console.addChannel(init.arena, {.echo = true});

    static botix::system::TelemetrySystem system_telemetry{{
        .config = system_config.device,
    }};

    static botix::system::TransportSystem system_transport{{
        .boot_transport_kind = system_config.user.boot_transport_kind,
    }};

    static botix::system::ProtocolSystem system_protocol{{
        .config = system_config.device,
        .transport_link = system_transport.link,
        .outgoing_telemetry = system_telemetry.outgoing,
        .cli_channel_output = maybe_other_channel.unwrap().output,
        .boot_protocol_kind = system_config.user.boot_protocol_kind,
    }};

    static botix::system::BehaviorSystem system_behavior{{
        .config = system_config.device,
        .periphery = periphery,
        .incoming_telemetry = system_telemetry.incoming,
    }};

    // system initialization

    // config

    system_config.setup(init.arena, console);

    // hardware

    (void) periphery.init();

    // telemetry

    system_telemetry.setup(init.arena, console);

    system_telemetry.outgoing.wheel_distance.callback([]() -> botix::OutgoingTelemetry::WheelDistance {
        return {
            .left_mm = periphery.wheel_odometry_encoder_left.positionUnits(),
            .right_mm = periphery.wheel_odometry_encoder_right.positionUnits(),
        };
    });

    // transport

    system_transport.setup(init.arena, console);

    system_transport.onReceive([&maybe_other_channel](auto const &context) -> void {
        system_protocol.link.receive({
            .transport = context,
            .incoming_telemetry = system_telemetry.incoming,
            .cli_channel_input = maybe_other_channel.unwrap().input,
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

    system_protocol.setup(init.arena, console);

    system_protocol.onRawFallback([&init](botix::transport::Address const &address, auto buffer) -> void {
        (void) address;
        init.logger.debug("raw (fallback): got {} bytes", buffer.length());
    });

    system_protocol.onMavlinkFallback([&init](botix::transport::Address const &address, auto const &message) -> void {
        (void) address;
        init.logger.debug("mavlink (fallback): msg id: {}, seq: {}", message.msgid, message.seq);
    });

    // behavior

    system_behavior.setup(init.arena, console);

    init.logger.info("Ready");

    // loop

    while (true) {
        constexpr auto loop_period{1000 / 100};

        if (maybe_main_channel.isSome()) {

            while (init.io.availableForRead() > 0) {
                if (auto const read = init.io.readByte(); read.isOk()) {
                    (void) maybe_main_channel.unwrap().input.feed({reinterpret_cast<char const *>(&read.ok()), 1});
                }
            }

            if (maybe_main_channel.unwrap().output.availableForRead() > 0) {
                auto const str = maybe_main_channel.unwrap().output.drain();
                (void) init.io.writeBuffer({reinterpret_cast<kf::u8 const *>(str.data()), str.length()});
            }
        }

        auto const now = rtos::Clock::now();

        system_telemetry.poll(now);
        system_transport.poll(now);
        system_protocol.poll(now);
        console.poll(now);
        system_behavior.poll(now);
        system_config.poll(now);

        rtos::Task::sleep(loop_period);
    }
}
