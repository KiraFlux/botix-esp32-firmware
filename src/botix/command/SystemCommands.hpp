// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>

#include <kf/Arena.hpp>
#include <kf/primitives.hpp>
#include <kf/rtos/Task.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/service/ConsoleService.hpp"
#include "botix/service/NetworkService.hpp"
#include "botix/service/NetworkState.hpp"

namespace botix::command {

/// @brief The `help`, `info` and `reboot` commands
struct SystemCommands : kf::mixin::NonCopyable {

    struct Dependencies {
        service::ConsoleService &console;
        service::NetworkService &network;
    };

    explicit constexpr SystemCommands(Dependencies deps) noexcept :
        _console{deps.console},
        _network{deps.network} {}

    [[nodiscard]] bool registerIn(service::ConsoleService &console, kf::Arena &arena) noexcept {
        if (console.addCommand(arena, "help", [this](auto const &call) { _console.printHelp(call.output); }).isNone()) {
            return false;
        }

        if (console.addCommand(arena, "info", [this](auto const &call) { info(call); }).isNone()) {
            return false;
        }

        return console.addCommand(arena, "reboot", [](auto const &call) { reboot(call.output); }).isSome();
    }

private:
    using Output = service::ConsoleService::Channel::Output;
    using Call = service::ConsoleService::Command::Context;

    /// @brief Time allowed for the output sink to drain before the reset lands
    static constexpr kf::units::Milliseconds _reboot_grace_ms{100};

    service::ConsoleService &_console;
    service::NetworkService &_network;

    void info(Call const &call) const noexcept {
        call.output.print("uptime_ms: {}", call.timestamp);
        call.output.print("free_heap: {}", static_cast<kf::u32>(ESP.getFreeHeap()));

        auto const state = _network.state();
        call.output.print("wifi: {}", service::name(state));

        if (state == service::NetworkState::Connected) {
            call.output.print("ip: {}", _network.localAddress());
        }
    }

    static void reboot(Output &output) noexcept {
        output.print("rebooting");
        output.flush();

        kf::rtos::Task::sleep(_reboot_grace_ms);
        ESP.restart();
    }
};

}// namespace botix::command
