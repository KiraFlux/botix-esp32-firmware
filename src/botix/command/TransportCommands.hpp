// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/primitives.hpp>

#include "botix/protocol/Kind.hpp"
#include "botix/service/ConsoleService.hpp"
#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TransportSystem.hpp"
#include "botix/transport/Kind.hpp"

namespace botix::command {

namespace internal {

enum class TransportAction : kf::u8 {
    Status,
    Use,
    Connect,
    Disconnect,
};

inline void printTransportStatus(
    service::ConsoleService::Channel::Output &output,
    system::TransportSystem &transport) noexcept {

    auto const kind = transport.link.kind();

    output.print(
        "transport: {}",
        kind.isNone()
            ? "none"
            : (kind.unwrap() == transport::Kind::Wifi ? "wifi" : "espnow"));

    output.print("connected: {}", transport.link.connected());

    auto const address = transport.link.activeAddress();
    if (address.isNone()) {
        output.print("peer: none");
        return;
    }

    auto const &peer = address.unwrap();

    if (peer.kind() == transport::Kind::Wifi) {
        auto const &endpoint = peer.endpoint();
        output.print(
            "peer: {}.{}.{}.{}:{}",
            endpoint.octet(0), endpoint.octet(1), endpoint.octet(2), endpoint.octet(3),
            endpoint.port);
    } else {
        auto const &mac = peer.mac();
        output.print(
            "peer: {}:{}:{}:{}:{}:{}",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

}// namespace internal

/// @brief Register `transport` and `protocol`
/// @return false when the console ran out of command or argument capacity
[[nodiscard]] inline bool registerTransportCommands(
    service::ConsoleService &console,
    kf::Arena &arena,
    system::TransportSystem &transport,
    system::ProtocolSystem &protocol) noexcept {

    struct Context {
        system::TransportSystem &transport;
        system::ProtocolSystem &protocol;
    };

    static Context context{transport, protocol};

    // transport <action> [kind]

    {
        auto maybe = console.addCommand(arena, "transport", [](auto const &call) {
            auto const action = static_cast<internal::TransportAction>(call.arguments[0].enumIndex());
            auto const kind = static_cast<transport::Kind>(call.arguments[1].enumIndex());

            switch (action) {
                case internal::TransportAction::Status:
                    internal::printTransportStatus(call.output, context.transport);
                    return;

                case internal::TransportAction::Use:
                    context.transport.use(kind);
                    call.output.print("active transport set; 'config set user.boot.transport' makes it persist");
                    internal::printTransportStatus(call.output, context.transport);
                    return;

                case internal::TransportAction::Connect: {
                    if (not context.transport.connectConfiguredWifi()) {
                        call.output.error("connect failed; check user.udp.remote_ip and remote_port");
                        return;
                    }
                    internal::printTransportStatus(call.output, context.transport);
                    return;
                }

                case internal::TransportAction::Disconnect:
                    context.transport.link.disconnect();
                    call.output.print("disconnected");
                    return;

                default:
                    call.output.error("unhandled action");
                    return;
            }
        });

        if (maybe.isNone()) { return false; }

        static service::ConsoleService::Command::Argument::EnumItem const actions[]{
            {"status", internal::TransportAction::Status},
            {"use", internal::TransportAction::Use},
            {"connect", internal::TransportAction::Connect},
            {"disconnect", internal::TransportAction::Disconnect},
        };

        static service::ConsoleService::Command::Argument::EnumItem const kinds[]{
            {"espnow", transport::Kind::Espnow},
            {"wifi", transport::Kind::Wifi},
        };

        if (not maybe.unwrap().addEnumArgument("action", {.items{actions}})) { return false; }

        // Only `use` consumes it, so it carries a default
        if (not maybe.unwrap().addEnumArgument(
                "kind",
                {
                    .params{.default_value = kf::some(static_cast<kf::usize>(transport::Kind::Espnow))},
                    .items{kinds},
                })) {
            return false;
        }
    }

    // protocol <kind>

    {
        auto maybe = console.addCommand(arena, "protocol", [](auto const &call) {
            auto const kind = static_cast<protocol::Kind>(call.arguments[0].enumIndex());

            context.protocol.link.set(context.protocol.get(kind));

            call.output.print(
                "protocol: {}",
                kind == protocol::Kind::Mavlink ? "mavlink" : "raw");
            call.output.print("note: 'config set user.boot.protocol' makes it persist");
        });

        if (maybe.isNone()) { return false; }

        static service::ConsoleService::Command::Argument::EnumItem const kinds[]{
            {"raw", protocol::Kind::Raw},
            {"mavlink", protocol::Kind::Mavlink},
        };

        if (not maybe.unwrap().addEnumArgument("kind", {.items{kinds}})) { return false; }
    }

    return true;
}

}// namespace botix::command
