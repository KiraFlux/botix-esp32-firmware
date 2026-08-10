// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <WiFi.h>

#include <kf/units.hpp>

#include "botix/service/NetworkService.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

/// @brief Owns the WiFi station link and the mDNS advertisement
struct NetworkSystem : System<NetworkSystem, void()> {

    struct Dependencies {
        service::NetworkService::Config const &config;
        /// @brief Port advertised over mDNS, owned by the UDP transport config
        kf::u16 const &service_port;
    };

    explicit constexpr NetworkSystem(Dependencies deps) noexcept :
        service{{
            .config = deps.config,
            .service_port = deps.service_port,
        }} {}

    service::NetworkService service;

private:
    BOTIX_IMPL_SYSTEM(NetworkSystem, void());

    void initImpl() noexcept {
        // ESP-NOW and the UDP transport both require the station interface
        (void) WiFi.mode(WIFI_MODE_STA);
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        service.poll(now);
    }
};

}// namespace botix::system
