// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/StringView.hpp>
#include <kf/units.hpp>

#include "botix/OutgoingTelemetry.hpp"
#include "botix/config/DeviceConfig.hpp"
#include "botix/protocol/Kind.hpp"
#include "botix/protocol/Link.hpp"
#include "botix/protocol/Protocol.hpp"
#include "botix/protocol/Registry.hpp"
#include "botix/transport/Link.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct ProtocolSystem : System<ProtocolSystem, void(protocol::Kind)> {

    struct Dependencies {
        config::DeviceConfig const &config;
        transport::Link &transport_link;
        OutgoingTelemetry &outgoing_telemetry;
    };

    explicit constexpr ProtocolSystem(Dependencies deps) noexcept :
        _registry{deps.config.protocol_registry},
        _protocol_poll_context{
            .transport_link = deps.transport_link,
            .outgoing_telemetry = deps.outgoing_telemetry,
            .timestamp = 0,
        } {}

    [[nodiscard]] auto &get(protocol::Kind kind) noexcept {
        return _registry.get(kind);
    }

    void onRawFallback(auto &&f) noexcept {
        _registry.raw.callback(std::forward<decltype(f)>(f));
    }

    void onMavlinkFallback(auto &&f) noexcept {
        _registry.mavlink.onMessageFallback(std::forward<decltype(f)>(f));
    }

    /// @brief Route console text arriving over MAVLink `SERIAL_CONTROL`
    void onSerialControl(auto &&f) noexcept {
        _registry.mavlink.onSerialControl(std::forward<decltype(f)>(f));
    }

    /// @brief Send console output back over `SERIAL_CONTROL`
    /// @note Only reaches the operator while the MAVLink protocol is the active one
    [[nodiscard]] bool sendSerialControl(kf::StringView text) noexcept {
        return _registry.mavlink.sendSerialControl(_protocol_poll_context.transport_link, text);
    }

    protocol::Link link{};

private:
    protocol::Registry _registry;
    protocol::Protocol::PollContext _protocol_poll_context;

    BOTIX_IMPL_SYSTEM(ProtocolSystem, void(protocol::Kind));

    void initImpl(protocol::Kind init_protocol_kind) noexcept {
        link.set(_registry.get(init_protocol_kind));
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        _protocol_poll_context.timestamp = now;
        link.poll(_protocol_poll_context);
    }
};

}// namespace botix::system