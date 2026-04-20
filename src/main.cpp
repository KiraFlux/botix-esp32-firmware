// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#include <kf/Logger.hpp>
#include <kf/Option.hpp>
#include <kf/gpio/arduino.hpp>
#include <kf/math/Timer.hpp>
#include <kf/math/units.hpp>
#include <kf/network/EspNow.hpp>

#include "botix/drivers/DRV8871.hpp"

using kf::network::EspNow;
using PwmOutput = kf::gpio::arduino::PwmOutput;

using DRV8871 = botix::drivers::DRV8871<PwmOutput>;

struct ControlPacket {
    kf::i16 left_x, left_y, right_x, right_y;
};

constexpr PwmOutput::Config makePwmConfig(gpio_num_t pin, kf::u8 channel) noexcept {
    return PwmOutput::Config{
        .frequency_hz = 20000,
        .resolution_bits = 8,
        .pin = static_cast<kf::u8>(pin),
        .channel = channel,
    };
}

static auto left_motor_forward_config{makePwmConfig(GPIO_NUM_32, 0)};
static auto left_motor_backward_config{makePwmConfig(GPIO_NUM_33, 1)};

static auto right_motor_forward_config{makePwmConfig(GPIO_NUM_25, 2)};
static auto right_motor_backward_config{makePwmConfig(GPIO_NUM_26, 3)};

DRV8871::Config motor_config{
    .max_input = 1000,
    .forward_dead_zone = 10,
    .backward_dead_zone = 10,
};

DRV8871 left_motor{
    motor_config,
    PwmOutput{left_motor_forward_config},
    PwmOutput{left_motor_backward_config},
};

DRV8871 right_motor{
    motor_config,
    PwmOutput{right_motor_forward_config},
    PwmOutput{right_motor_backward_config},
};

kf::Option<EspNow::Peer> broadcast_peer{};

kf::math::Timer heartbeat_timer{static_cast<kf::math::Milliseconds>(1000)};
kf::math::Timer control_update_timer{static_cast<kf::math::Milliseconds>(100)};
kf::math::Timer control_timeout_timer{static_cast<kf::math::Milliseconds>(1000)};

volatile bool got_packet{false};
int current_left_input = 0;
int current_right_input = 0;

void onDataRecv(const EspNow::Mac &mac, kf::memory::Slice<const kf::u8> buffer) {
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

    if (not left_motor.init()) {
        logger.error("Left motor init fail");
        return;
    }
    left_motor.set(0);

    if (not right_motor.init()) {
        logger.error("Right motor init fail");
        return;
    }
    right_motor.set(0);

    auto &e{EspNow::instance()};

    const auto init_result{e.init()};
    if (init_result.isError()) {
        logger.error("Espnow init failed");
        return;
    }

    (void) e.onReceiveFromUnknown(onDataRecv);

    auto peer_result{EspNow::Peer::add(EspNow::Mac{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})};
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

        left_motor.set(current_left_input);
        right_motor.set(current_right_input);
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
