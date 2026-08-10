// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/protocol/Kind.hpp"
#include "botix/service/ConsoleService.hpp"
#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/TransportSystem.hpp"
#include "botix/transport/Kind.hpp"

namespace botix::command {

/// @brief The `transport` and `protocol` commands
struct TransportCommands : kf::mixin::NonCopyable {

    struct Dependencies {
        system::TransportSystem &transport;
        system::ProtocolSystem &protocol;
    };

    explicit constexpr TransportCommands(Dependencies deps) noexcept :
        _transport{deps.transport},
        _protocol{deps.protocol} {}

    [[nodiscard]] bool registerIn(service::ConsoleService &console, kf::Arena &arena) noexcept {
        return registerTransport(console, arena) and registerProtocol(console, arena);
    }

private:
    using Output = service::ConsoleService::Channel::Output;
    using Call = service::ConsoleService::Command::Context;
    using EnumItem = service::ConsoleService::Command::Argument::EnumItem;

    enum class Action : kf::u8 {
        Status,
        Use,
        Connect,
        Disconnect,
    };

    static constexpr EnumItem _actions[]{
        {"status", Action::Status},
        {"use", Action::Use},
        {"connect", Action::Connect},
        {"disconnect", Action::Disconnect},
    };

    static constexpr EnumItem _transport_kinds[]{
        {"espnow", transport::Kind::Espnow},
        {"wifi", transport::Kind::Wifi},
    };

    static constexpr EnumItem _protocol_kinds[]{
        {"raw", protocol::Kind::Raw},
        {"mavlink", protocol::Kind::Mavlink},
    };

    system::TransportSystem &_transport;
    system::ProtocolSystem &_protocol;

    [[nodiscard]] bool registerTransport(service::ConsoleService &console, kf::Arena &arena) noexcept {
        auto maybe = console.addCommand(arena, "transport", [this](auto const &call) { executeTransport(call); });

        if (maybe.isNone()) {
            return false;
        }

        auto &command = maybe.unwrap();

        if (not command.addEnumArgument("action", {.items{_actions}})) { return false; }

        // Only `use` consumes it, so it carries a default
        return command.addEnumArgument(
            "kind",
            {
                .params{.default_value = kf::some(static_cast<kf::usize>(transport::Kind::Espnow))},
                .items{_transport_kinds},
            });
    }

    [[nodiscard]] bool registerProtocol(service::ConsoleService &console, kf::Arena &arena) noexcept {
        auto maybe = console.addCommand(arena, "protocol", [this](auto const &call) { executeProtocol(call); });

        if (maybe.isNone()) {
            return false;
        }

        return maybe.unwrap().addEnumArgument("kind", {.items{_protocol_kinds}});
    }

    void executeTransport(Call const &call) noexcept {
        auto const action = static_cast<Action>(call.arguments[0].enumIndex());
        auto const kind = static_cast<transport::Kind>(call.arguments[1].enumIndex());

        switch (action) {
            case Action::Status:
                return printStatus(call.output);

            case Action::Use:
                _transport.use(kind);
                call.output.print("active transport set; 'config set user.boot.transport' makes it persist");
                return printStatus(call.output);

            case Action::Connect:
                if (not _transport.connectConfiguredWifi()) {
                    call.output.error("connect failed; check user.udp.remote_ip and remote_port");
                    return;
                }
                return printStatus(call.output);

            case Action::Disconnect:
                _transport.link.disconnect();
                return call.output.print("disconnected");

            default:
                return call.output.error("unhandled action");
        }
    }

    void executeProtocol(Call const &call) noexcept {
        auto const kind = static_cast<protocol::Kind>(call.arguments[0].enumIndex());

        _protocol.link.set(_protocol.get(kind));

        call.output.print("protocol: {}", protocol::name(kind));
        call.output.print("note: 'config set user.boot.protocol' makes it persist");
    }

    void printStatus(Output &output) const noexcept {
        auto const kind = _transport.link.kind();

        output.print("transport: {}", kind.isNone() ? kf::StringView{"none"} : transport::name(kind.unwrap()));
        output.print("connected: {}", _transport.link.connected());

        auto const address = _transport.link.activeAddress();

        if (address.isNone()) {
            output.print("peer: none");
        } else {
            output.print("peer: {}", address.unwrap());
        }
    }
};

}// namespace botix::command
