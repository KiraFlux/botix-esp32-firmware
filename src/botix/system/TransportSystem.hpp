// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/Option.hpp>
#include <kf/units.hpp>

#include "botix/transport/Address.hpp"
#include "botix/transport/Kind.hpp"
#include "botix/transport/Link.hpp"
#include "botix/transport/Receiver.hpp"
#include "botix/transport/Registry.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct TransportSystem : System<TransportSystem, void(transport::Kind)> {

    struct Dependencies {
        transport::Registry::Config const &config;
    };

    explicit constexpr TransportSystem(Dependencies deps) noexcept :
        _registry{deps.config} {}

    [[nodiscard]] auto &get(transport::Kind kind) noexcept {
        return _registry.get(kind);
    }

    void onReceive(auto &&f) noexcept {
        _receiver.onReceive(std::forward<decltype(f)>(f));
    }

    void onReceiveForeign(auto &&f) noexcept {
        _receiver.onReceiveForeign(std::forward<decltype(f)>(f));
    }

    /// @brief Switch the active transport, dropping any current connection
    void use(transport::Kind kind) noexcept {
        link.set(_registry.get(kind));
    }

    /// @brief Connect the WiFi transport to the endpoint held in configuration
    /// @return false when no remote endpoint is configured
    [[nodiscard]] bool connectConfiguredWifi() noexcept {
        return link.connect(_registry.wifi_udp.configuredAddress());
    }

    transport::Link link{};

private:
    transport::Receiver _receiver{};
    transport::Registry _registry;

    BOTIX_IMPL_SYSTEM(TransportSystem, void(transport::Kind));

    void initImpl(transport::Kind init_transport_kind) noexcept {
        (void) _registry.espnow.init();
        (void) _registry.wifi_udp.init();

        _registry.espnow.receiver(kf::someRef(_receiver));
        _registry.wifi_udp.receiver(kf::someRef(_receiver));

        link.set(_registry.get(init_transport_kind));

        // The WiFi peer is configured rather than discovered, so it can be
        // bound immediately; the socket itself binds once the station is up.
        if (init_transport_kind == transport::Kind::Wifi) {
            (void) connectConfiguredWifi();
        }
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        link.poll(now);
    }
};

}// namespace botix::system
