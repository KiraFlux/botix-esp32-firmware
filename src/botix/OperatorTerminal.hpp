// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <WiFi.h>

#include <kf/Logger.hpp>
#include <kf/Option.hpp>
#include <kf/Timer.hpp>
#include <kf/units.hpp>
#include <kf/esp/Espnow.hpp>
#include <kf/MacAddress.hpp>

#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

#include "botix/Control.hpp"

namespace botix {

/// @brief Permanent ESP‑NOW service for interactive robot control and telemetry.
/// @details Lets an operator drive, monitor, and tweak the robot in real time
struct OperatorTerminal final :

    ::kf::mixin::Initable<OperatorTerminal, bool()>,
    ::kf::mixin::NonCopyable,
    ::kf::mixin::TimedPollable<OperatorTerminal>

{
    using Self = OperatorTerminal;

    explicit OperatorTerminal(Control &control) noexcept :
        _control{control} {}

private:
    using Espnow = kf::esp::Espnow;

    inline static kf::Logger logger{"OperatorTerminal"};

    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 1000};

    Control &_control;
    kf::Option<Espnow::Peer> _broadcast_peer{};
    kf::Timer _heartbeat_timer{heartbeat_timer_config};


    KF_IMPL_INITABLE(Self, bool());
    bool initImpl() noexcept {
        if (not WiFi.mode(WIFI_MODE_STA)) {
            logger.error("WiFi mode failed");
            return false;
        }

        auto &espnow = Espnow::instance();

        const auto init_result = espnow.init();
        if (init_result.isError()) {
            logger.error("Espnow init failed");
            return false;
        }

        espnow.callback([this](const kf::MacAddress &mac, kf::Slice<const kf::u8> buffer) -> void {
            switch (buffer.length()) {
                case sizeof(Control::Input):
                    _control.input(*reinterpret_cast<const Control::Input *>(buffer.data()));
                    return;

                default:
                    return;
            }
        });

        auto peer_result = Espnow::Peer::create({
            .mac_address = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
            .wifi_interface_sta = true,
        });

        if (peer_result.isOk()) {
            _broadcast_peer = kf::some(std::move(peer_result.ok()));
        } else {
            logger.error("Espnow peer add failed");
        }

        return true;
    }

    KF_IMPL_TIMED_POLLABLE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);

            logger.debug("heartbeat");

            if (_broadcast_peer.isSome()) {
                (void) _broadcast_peer.unwrap().writeByte(0xAA);
                logger.debug("send");
            }

            // Serial.printf("L:\t%d\tR:\t%d\n", int(periphery.wheel_odometry_encoder_left.positionTicks()), int(periphery.wheel_odometry_encoder_right.positionTicks()));
        }
    }
};

}// namespace botix