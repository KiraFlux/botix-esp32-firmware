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

#include "botix/Control.hpp"
#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"

#include "botix/transport/Registry.hpp"
#include "botix/transport/Link.hpp"

#include "botix/protocol/Link.hpp"
#include "botix/protocol/Registry.hpp"

static botix::RootConfig root_config{};

static botix::transport::Registry transport_registry{};

static botix::transport::Link transport_link{};

static botix::protocol::Registry protocol_registry{};

static botix::protocol::Link protocol_link{};

static botix::Periphery periphery{
    root_config.periphery,
};

static botix::Control control{
    root_config.control,
};

void onTransportReceive(kf::MacAddress const &mac, kf::BytesView buffer) {
    (void) mac;
    protocol_link.receive(buffer);
}

void onRawProtocolReceive(kf::BytesView buffer) {
    switch (buffer.length()) {
        case sizeof(botix::Control::Input):
            control.input(*reinterpret_cast<botix::Control::Input const *>(buffer.data()));
            return;

        default:
            return;
    }
}

void onMavlinkProtocolReceive(mavlink_message_t const &message) {
    switch (message.msgid) {
        case MAVLINK_MSG_ID_MANUAL_CONTROL: {
            mavlink_manual_control_t m;
            mavlink_msg_manual_control_decode(&message, &m);

            control.input(botix::Control::Input{
                .left_x = m.r,
                .left_y = m.z,
                .right_x = m.y,
                .right_y = m.x,
            });

            return;
        }
    }
}

void onInputChar(kf::Init &init, char c) {
    switch (c) {
        case 'o': {
            init.logger.debug(
                "Encoders: \tL: {} \t R: {}",
                periphery.wheel_odometry_encoder_left.positionTicks(),
                periphery.wheel_odometry_encoder_right.positionTicks());
            return;
        }

        case 'r': {
            protocol_link.set(protocol_registry.raw());
            init.logger.debug("protocol: Raw");
            return;
        }

        case 'm': {
            protocol_link.set(protocol_registry.mavlink());
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

    if (not transport_registry.espnow().init()) {
        init.logger.warn("EspnowTransport init failed");
        return;
    }

    // espnow_transport.callback(onTransportReceive);
    protocol_registry.raw().callback(onRawProtocolReceive);
    protocol_registry.mavlink().callback(onMavlinkProtocolReceive);

    protocol_link.set(protocol_registry.mavlink());

    init.logger.info("Ready");

    while (true) {
        constexpr auto loop_period{1000 / 100};// 10 Hz Loop rate

        auto const now = rtos::Clock::now();

        protocol_link.poll(now, transport_link);
        control.poll(now);

        {
            auto const &o = control.output();

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
