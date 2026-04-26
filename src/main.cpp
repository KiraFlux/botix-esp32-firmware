// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>

#include <kf/Logger.hpp>
#include <kf/Option.hpp>
#include <kf/math/Timer.hpp>
#include <kf/math/units.hpp>

#include "botix/Periphery.hpp"

struct ControlPacket {
    kf::i16 left_x, left_y, right_x, right_y;
};

static auto periphery_config{botix::Periphery::Config::defaults()};

static botix::Periphery periphery{
    periphery_config,
};

kf::Option<botix::EspNow::Peer> broadcast_peer{};

kf::math::Timer heartbeat_timer{static_cast<kf::math::Milliseconds>(1000)};
kf::math::Timer control_update_timer{static_cast<kf::math::Milliseconds>(100)};
kf::math::Timer control_timeout_timer{static_cast<kf::math::Milliseconds>(1000)};

volatile bool got_packet{false};
int current_left_input = 0;
int current_right_input = 0;

void onDataRecv(const botix::EspNow::Mac &mac, kf::memory::Slice<const kf::u8> buffer) {
    got_packet = true;

    if (buffer.size() == sizeof(ControlPacket)) {
        ControlPacket p;
        memcpy(&p, buffer.data(), sizeof(p));

        current_left_input = p.left_y + p.left_x;
        current_right_input = p.left_y - p.left_x;
    }
}

void setup() {
    constexpr auto logger{kf::Logger::create("setup")};
    kf::Logger::writer = [](kf::memory::StringView str) { Serial.write(str.data(), str.size()); };

    Serial.begin(115200);

    logger.debug("starting");

    if (not periphery.init()) {
        logger.error("Periphery init failed");
        return;
    }

    periphery.motor_driver_left.stop();
    periphery.motor_driver_right.stop();

    auto &e{botix::EspNow::instance()};

    const auto init_result{e.init()};
    if (init_result.isError()) {
        logger.error("Espnow init failed");
        return;
    }

    (void) e.onReceiveFromUnknown(onDataRecv);

    auto peer_result{botix::EspNow::Peer::add(botix::EspNow::Mac{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})};
    if (peer_result.isOk()) {
        broadcast_peer.value(std::move(peer_result.value()));
    } else {
        logger.error("Espnow peer add failed");
    }

    const auto now{static_cast<kf::math::Milliseconds>(millis())};
    heartbeat_timer.start(now);
    control_update_timer.start(now);

    logger.info("Ready");
}

void loop() {
    const auto now{static_cast<kf::math::Milliseconds>(millis())};

    if (heartbeat_timer.expired(now)) {
        heartbeat_timer.start(now);

        if (broadcast_peer.hasValue()) {
            (void) broadcast_peer.value().writeByte(0xAA);
        }
    }

    if (control_update_timer.expired(now)) {
        control_update_timer.start(now);

        periphery.motor_driver_left.set(current_left_input);
        periphery.motor_driver_right.set(current_right_input);

        // Serial.printf("L:\t%d\tR:\t%d\n", int(periphery.wheel_odometry_encoder_left.positionTicks()), int(periphery.wheel_odometry_encoder_right.positionTicks()));
    }

    if (got_packet) {
        control_timeout_timer.start(now);
        got_packet = false;
    }

    if (control_timeout_timer.expired(now)) {
        current_left_input = 0;
        current_right_input = 0;
    }
}
