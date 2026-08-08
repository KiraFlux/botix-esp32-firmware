// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/Option.hpp>
#include <kf/units.hpp>

#include "botix/transport/Link.hpp"
#include "botix/transport/Receiver.hpp"
#include "botix/transport/Registry.hpp"
#include "botix/transport/Kind.hpp"
#include "botix/transport/Transport.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct TransportSystem : System<TransportSystem, void(transport::Kind)> {

    [[nodiscard]] transport::Link &link() noexcept {
        return _link;
    }

    [[nodiscard]] transport::Transport &get(transport::Kind kind) noexcept {
        return _registry.get(kind);
    }

    void onReceive(auto &&f) noexcept {
        _receiver.onReceive(std::forward<decltype(f)>(f));
    }

    void onReceiveForeign(auto &&f) noexcept {
        _receiver.onReceiveForeign(std::forward<decltype(f)>(f));
    }

private:
    transport::Link _link{};
    transport::Receiver _receiver{};
    transport::Registry _registry{};

    BOTIX_IMPL_SYSTEM(TransportSystem, void(transport::Kind));

    void initImpl(transport::Kind init_transport_kind) noexcept {
        (void) _registry.espnow().init();

        _registry.espnow().receiver(kf::someRef(_receiver));
        
        _link.set(_registry.get(init_transport_kind));
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        _link.poll(now);
    }
};

}// namespace botix::system