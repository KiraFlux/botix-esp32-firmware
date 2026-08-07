// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <WiFi.h>

#include <kf/Logger.hpp>
#include <kf/MacAddress.hpp>
#include <kf/Option.hpp>
#include <kf/Timer.hpp>
#include <kf/esp/Espnow.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

#include "botix/Control.hpp"

namespace botix::transport {

struct EspnowTransport final :

    kf::mixin::Initable<EspnowTransport, bool()>,
    kf::mixin::NonCopyable,
    kf::mixin::TimedPollable<EspnowTransport>

{
    using Self = EspnowTransport;

    void callback(auto &&f) noexcept {
        Espnow::instance().callback(std::forward<decltype(f)>(f));
    }

private:
    using Espnow = kf::esp::Espnow;

    inline static kf::Logger logger{"EspnowTransport"};

    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 2000};

    kf::Option<Espnow::Peer> _broadcast_peer{};
    kf::Timer _heartbeat_timer{heartbeat_timer_config};

    KF_IMPL_INITABLE(Self, bool());
    bool initImpl() noexcept {
        if (not WiFi.mode(WIFI_MODE_STA)) {
            logger.error("WiFi mode failed");
            return false;
        }

        auto &espnow = Espnow::instance();

        auto const init_result = espnow.init();
        if (init_result.isError()) {
            logger.error("Espnow init failed: {}", init_result.error());
            return false;
        }

        auto peer_result = Espnow::Peer::create({
            .mac_address = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
            .wifi_interface_sta = true,
        });

        if (peer_result.isOk()) {
            _broadcast_peer = kf::some(std::move(peer_result.ok()));
        } else {
            logger.error("Espnow peer add failed: {}", peer_result.error());
        }

        return true;
    }

    KF_IMPL_TIMED_POLLABLE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);

            if (_broadcast_peer.isSome()) {
                (void) _broadcast_peer.unwrap().writeByte(0xAA);
            }
        }
    }
};

}// namespace botix