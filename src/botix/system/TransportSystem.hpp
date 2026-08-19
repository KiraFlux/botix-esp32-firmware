// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <WiFi.h>

#include <kf/Arena.hpp>
#include <kf/Option.hpp>
#include <kf/units.hpp>

#include "botix/cli/Group.hpp"
#include "botix/transport/Kind.hpp"
#include "botix/transport/Link.hpp"
#include "botix/transport/Receiver.hpp"
#include "botix/transport/Registry.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct TransportSystem : System<TransportSystem> {

    struct Dependencies {
        transport::Kind init_transport_kind;
    };

    explicit constexpr TransportSystem(Dependencies deps) noexcept :
        System<TransportSystem>{{.name{"transport"}}},
        link{_registry.get(deps.init_transport_kind)} {}

    [[nodiscard]] auto &get(transport::Kind kind) noexcept {
        return _registry.get(kind);
    }

    void onReceive(auto &&f) noexcept {
        _receiver.onReceive(std::forward<decltype(f)>(f));
    }

    void onReceiveForeign(auto &&f) noexcept {
        _receiver.onReceiveForeign(std::forward<decltype(f)>(f));
    }

private:
    transport::Receiver _receiver{};
    transport::Registry _registry{};

public:
    transport::Link link;

private:
    BOTIX_IMPL_SYSTEM(TransportSystem);

    void onSetupImpl() noexcept {
        WiFi.mode(WIFI_MODE_STA);

        (void) _registry.espnow.init();

        _registry.espnow.receiver(kf::someRef(_receiver));
    }

    void setupCliImpl(kf::Arena &arena, cli::Group &group) noexcept {}

    void pollImpl(kf::units::Milliseconds now) noexcept {
        link.poll(now);
    }
};

}// namespace botix::system