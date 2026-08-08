// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <MAVLink.h>
#include <WiFi.h>

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include <kf/BytesView.hpp>
#include <kf/Logger.hpp>
#include <kf/units.hpp>

#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"

#include "botix/protocol/Kind.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/service/MixerService.hpp"

#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TelemetrySystem.hpp"
#include "botix/system/TransportSystem.hpp"

static char test_log_buffer[256];
static kf::Logger test_log{"test", {test_log_buffer}};

static botix::RootConfig root_config{};

static botix::system::TelemetrySystem telemetry_system{
    root_config,
};

static botix::system::TransportSystem transport_system{};

static botix::system::ProtocolSystem protocol_system{
    root_config,
    transport_system.link(),
    telemetry_system.outgoing(),
};

static botix::Periphery periphery{
    root_config.periphery,
};

static botix::service::MixerService mixer_service{
    root_config.mixer_service,
    telemetry_system.incoming().control_input,
};

botix::OutgoingTelemetry::WheelDistance getWheelDistance() noexcept {
    return {
        .left_mm = periphery.wheel_odometry_encoder_left.positionUnits(),
        .right_mm = periphery.wheel_odometry_encoder_right.positionUnits(),
    };
}

void onTransportReceive(botix::transport::Receiver::ReceiveContext const &context) noexcept {
    protocol_system.link().receive({
        .transport = context,
        .incoming_telemetry = telemetry_system.incoming(),
        .timestamp = kf::rtos::Clock::now(),
    });
}

void onTransportReceiveForeign(botix::transport::Receiver::ReceiveContext const &context) noexcept {
    test_log.debug("Found device");

    if (transport_system.link().connected()) {
        test_log.error("connect denied (already connected)");
        return;
    }

    if (not transport_system.link().connect(context.address)) {
        test_log.error("transport_system.link().connect failed");
    }
}

void onRawProtocolFallback(botix::transport::Address const &address, kf::BytesView buffer) noexcept {
    (void) address;

    test_log.debug("raw (fallback): got {} bytes", buffer.length());
}

void onMavlinkProtocolFallback(botix::transport::Address const &address, mavlink_message_t const &message) noexcept {
    (void) address;

    test_log.debug("mavlink (fallback): msg id: {}, seq: {}", message.msgid, message.seq);
}

void onInputChar(kf::Init &init, char c) noexcept {
    switch (c) {
        case 'o': {
            init.logger.debug(
                "Encoders: \tL: {} \t R: {}",
                periphery.wheel_odometry_encoder_left.positionTicks(),
                periphery.wheel_odometry_encoder_right.positionTicks());
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
}

void kf::main(kf::Init &init) {
    init.logger.debug("Starting");

    root_config.reset();// set to defaults

    if (not periphery.init()) {
        init.logger.error("Periphery init failed");
        return;
    }

    if (not WiFi.mode(WIFI_MODE_STA)) {
        init.logger.error("WiFi mode failed");
        return;
    }

    telemetry_system.init();
    telemetry_system.outgoing().wheel_distance.callback(getWheelDistance);

    transport_system.init(botix::transport::Kind::Espnow);
    transport_system.onReceive(onTransportReceive);
    transport_system.onReceiveForeign(onTransportReceiveForeign);

    protocol_system.init(botix::protocol::Kind::Mavlink);
    protocol_system.onRawFallback(onRawProtocolFallback);
    protocol_system.onMavlinkFallback(onMavlinkProtocolFallback);

    init.logger.info("Ready");

    while (true) {
        constexpr auto loop_period{1000 / 100};// 10 Hz Loop rate

        auto const now = rtos::Clock::now();

        telemetry_system.poll(now);
        transport_system.poll(now);
        protocol_system.poll(now);

        mixer_service.poll(now);
        {
            auto const &o = mixer_service.output();

            periphery.motor_driver_left.set(o.motor_left_set);
            periphery.motor_driver_right.set(o.motor_right_set);
        }

        {
            while (init.io.availableForRead() > 0) {
                if (auto const read = init.io.readPacket<char>(); read.isOk()) {
                    onInputChar(init, read.ok());
                }
            }
        }

        rtos::Task::sleep(loop_period);
    }
}
