// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include <kf/Logger.hpp>
#include <kf/units.hpp>

#include "botix/Control.hpp"
#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"

#include "botix/transport/EspnowTransport.hpp"

#include "botix/protocol/ProtocolLink.hpp"
#include "botix/protocol/ProtocolRegistry.hpp"

static botix::RootConfig root_config{};

static botix::transport::EspnowTransport espnow_transport{};

static botix::protocol::ProtocolRegistry protocol_registry{};

static botix::protocol::ProtocolLink protocol_link{};

static botix::Periphery periphery{
    root_config.periphery,
};

static botix::Control control{
    root_config.control,
};

void onTransportReceive(kf::MacAddress const &mac, kf::Slice<kf::u8 const> buffer) {
    (void) mac;
    protocol_link.receive(buffer);
}

void onRawProtocolReceive(kf::Slice<kf::u8 const> buffer) {
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
            protocol_link.protocol(protocol_registry.raw());
            init.logger.debug("protocol: Raw");
            return;
        }

        case 'm': {
            protocol_link.protocol(protocol_registry.mavlink());
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

    if (not espnow_transport.init()) {
        init.logger.warn("EspnowTransport init failed");
        return;
    }

    espnow_transport.callback(onTransportReceive);
    protocol_registry.raw().callback(onRawProtocolReceive);
    protocol_registry.mavlink().callback(onMavlinkProtocolReceive);

    protocol_link.protocol(protocol_registry.mavlink());

    init.logger.info("Ready");

    while (true) {
        constexpr auto loop_period{1000 / 100};// 10 Hz Loop rate

        auto const now = rtos::Clock::now();

        protocol_link.poll(now, espnow_transport);
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
