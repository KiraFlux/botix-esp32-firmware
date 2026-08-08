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

#include "botix/protocol/Kind.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/system/TransportSystem.hpp"
#include "botix/system/ProtocolSystem.hpp"

static char test_log_buffer[256];
static kf::Logger test_log{"test", {test_log_buffer}};

static botix::RootConfig root_config{};

static botix::system::TransportSystem transport_system{};

static botix::system::ProtocolSystem protocol_system{
    transport_system.link(),
};

static botix::Periphery periphery{
    root_config.periphery,
};

static botix::Control control{
    root_config.control,
};

void onTransportReceive(botix::transport::Address const &address, kf::BytesView buffer) {
    protocol_system.link().receive(address, buffer);

    test_log.debug("onTransportReceive: {}", buffer.length());
}

void onTransportReceiveForeign(botix::transport::Address const &address, kf::BytesView buffer) {
    test_log.debug("Found device");

    if (transport_system.link().connected()) {
        test_log.error("connect denied (already connected)");
        return;
    }

    if (not transport_system.link().connect(address)) {
        test_log.error("transport_system.link().connect failed");
    }
}

void onRawProtocolReceive(botix::transport::Address const &address, kf::BytesView buffer) {
    (void) address;

    switch (buffer.length()) {
        case sizeof(botix::Control::Input):
            control.input(*reinterpret_cast<botix::Control::Input const *>(buffer.data()));
            return;

        default:
            return;
    }
}

void onMavlinkProtocolReceive(botix::transport::Address const &address, mavlink_message_t const &message) {
    (void) address;

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

    transport_system.init(botix::transport::Kind::Espnow);
    transport_system.onReceive(onTransportReceive);
    transport_system.onReceiveForeign(onTransportReceiveForeign);

    protocol_system.init(botix::protocol::Kind::Mavlink);
    protocol_system.onRawReceive(onRawProtocolReceive);
    protocol_system.onMavlinkReceive(onMavlinkProtocolReceive);

    init.logger.info("Ready");

    while (true) {
        constexpr auto loop_period{1000 / 100};// 10 Hz Loop rate

        auto const now = rtos::Clock::now();

        transport_system.poll(now);
        protocol_system.poll(now);
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
