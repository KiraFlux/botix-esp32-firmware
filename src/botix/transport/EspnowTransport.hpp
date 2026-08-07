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

#include "botix/Control.hpp"

namespace botix::transport {

struct EspnowTransport final :

    kf::mixin::Initable<EspnowTransport, bool()>,
    kf::mixin::NonCopyable

{
    using Self = EspnowTransport;

    void callback(auto &&f) noexcept {
        Espnow::instance().callback(std::forward<decltype(f)>(f));
    }

    [[nodiscard]] bool sendPacket(auto const &packet) noexcept {
        if (_broadcast_peer.isSome()) {
            return _broadcast_peer.unwrap().writePacket(packet).isOk();
        }
        return false;
    }

    [[nodiscard]] bool sendBuffer(kf::Slice<kf::u8 const> buffer) noexcept {
        if (_broadcast_peer.isSome()) {
            return _broadcast_peer.unwrap().writeBuffer(buffer).isOk();
        }
        return false;
    }

private:
    using Espnow = kf::esp::Espnow;

    inline static kf::Logger logger{"EspnowTransport"};

    kf::Option<Espnow::Peer> _broadcast_peer{};

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
};

}// namespace botix