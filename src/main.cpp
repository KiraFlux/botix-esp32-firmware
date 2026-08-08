// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include "botix/RootConfig.hpp"

#include "botix/protocol/Kind.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/system/BehaviorSystem.hpp"
#include "botix/system/PeripherySystem.hpp"
#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TelemetrySystem.hpp"
#include "botix/system/TransportSystem.hpp"

static botix::RootConfig root_config{};

void kf::main(kf::Init &init) {
    init.logger.debug("Starting");

    root_config.reset();// set to defaults

    // system instances

    static botix::system::PeripherySystem periphery_system{
        root_config,
    };

    static botix::system::TelemetrySystem telemetry_system{
        root_config,
    };

    static botix::system::BehaviorSystem behavior_system{
        root_config,
        periphery_system.periphery(),
        telemetry_system.incoming(),
    };

    static botix::system::TransportSystem transport_system{};

    static botix::system::ProtocolSystem protocol_system{
        root_config,
        transport_system.link(),
        telemetry_system.outgoing(),
    };

    // system initialization

    // periphery

    periphery_system.init();

    // telemetry

    telemetry_system.init();

    telemetry_system.outgoing().wheel_distance.callback([]() -> botix::OutgoingTelemetry::WheelDistance {
        return {
            .left_mm = periphery_system.periphery().wheel_odometry_encoder_left.positionUnits(),
            .right_mm = periphery_system.periphery().wheel_odometry_encoder_right.positionUnits(),
        };
    });

    // behavior

    behavior_system.init();

    // transport

    transport_system.init(botix::transport::Kind::Espnow);

    transport_system.onReceive([](auto const &context) -> void {
        protocol_system.link().receive({
            .transport = context,
            .incoming_telemetry = telemetry_system.incoming(),
            .timestamp = kf::rtos::Clock::now(),
        });
    });

    transport_system.onReceiveForeign([&init](auto const &context) -> void {
        init.logger.debug("Found device");

        if (transport_system.link().connected()) {
            init.logger.error("connect denied (already connected)");
            return;
        }

        if (not transport_system.link().connect(context.address)) {
            init.logger.error("transport_system.link().connect failed");
        }
    });

    // protocol

    protocol_system.init(botix::protocol::Kind::Mavlink);

    protocol_system.onRawFallback([&init](auto const &address, auto buffer) -> void {
        (void) address;
        init.logger.debug("raw (fallback): got {} bytes", buffer.length());
    });

    protocol_system.onMavlinkFallback([&init](auto const &address, auto const &message) -> void {
        (void) address;
        init.logger.debug("mavlink (fallback): msg id: {}, seq: {}", message.msgid, message.seq);
    });

    init.logger.info("Ready");

    // loop

    auto const on_input_char = [&init](char c) -> void {
        switch (c) {
            case 'o': {
                init.logger.debug(
                    "Encoders: \tL: {} \t R: {}",
                    periphery_system.periphery().wheel_odometry_encoder_left.positionTicks(),
                    periphery_system.periphery().wheel_odometry_encoder_right.positionTicks());
                return;
            }

            case 'r': {
                protocol_system.link().set(protocol_system.get(botix::protocol::Kind::Raw));
                init.logger.debug("protocol: Raw");
                return;
            }

            case 'm': {
                protocol_system.link().set(protocol_system.get(botix::protocol::Kind::Mavlink));
                init.logger.debug("protocol: Mavlink");
                return;
            }
        }
    };

    while (true) {
        constexpr auto loop_period{1000 / 100};

        auto const now = rtos::Clock::now();

        telemetry_system.poll(now);
        transport_system.poll(now);
        protocol_system.poll(now);
        behavior_system.poll(now);
        periphery_system.poll(now);

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
