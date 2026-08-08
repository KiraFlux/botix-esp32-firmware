// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <MAVLink.h>

#include <kf/units.hpp>

#include "botix/OutgoingTelemetry.hpp"
#include "botix/RootConfig.hpp"
#include "botix/protocol/Kind.hpp"
#include "botix/protocol/Link.hpp"
#include "botix/protocol/Protocol.hpp"
#include "botix/protocol/Registry.hpp"
#include "botix/transport/Link.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct ProtocolSystem : System<ProtocolSystem, void(protocol::Kind)> {

    explicit constexpr ProtocolSystem(
        RootConfig const &config,
        transport::Link &transport_link,
        OutgoingTelemetry &outgoing_telemetry) noexcept :
        _transport_link{transport_link},
        _outgoing_telemetry{outgoing_telemetry},
        _registry{config.protocol_registry}

    {}

    [[nodiscard]] protocol::Link &link() noexcept {
        return _protocol_link;
    }

    [[nodiscard]] protocol::Protocol &get(protocol::Kind kind) noexcept {
        return _registry.get(kind);
    }

    void onRawFallback(auto &&f) noexcept {
        _registry.raw.callback(std::forward<decltype(f)>(f));
    }

    void onMavlinkFallback(auto &&f) noexcept {
        _registry.mavlink.callback(std::forward<decltype(f)>(f));
    }

private:
    transport::Link &_transport_link;
    OutgoingTelemetry &_outgoing_telemetry;

    protocol::Link _protocol_link{};
    protocol::Registry _registry;

    BOTIX_IMPL_SYSTEM(ProtocolSystem, void(protocol::Kind));

    void initImpl(protocol::Kind init_protocol_kind) noexcept {
        _protocol_link.set(_registry.get(init_protocol_kind));
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        _protocol_link.poll({
            .transport_link = _transport_link,
            .outgoing_telemetry = _outgoing_telemetry,
            .timestamp = now,
        });
    }
};

}// namespace botix::system