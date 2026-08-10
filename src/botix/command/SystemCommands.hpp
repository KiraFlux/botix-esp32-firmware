// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>

#include <kf/Arena.hpp>
#include <kf/primitives.hpp>

#include "botix/service/ConsoleService.hpp"
#include "botix/service/NetworkService.hpp"

namespace botix::command {

/// @brief Register `help`, `info` and `reboot`
/// @return false when the console ran out of command or argument capacity
[[nodiscard]] inline bool registerSystemCommands(
    service::ConsoleService &console,
    kf::Arena &arena,
    service::NetworkService &network) noexcept {

    struct Context {
        service::ConsoleService &console;
        service::NetworkService &network;
    };

    static Context context{console, network};

    // help

    {
        auto maybe = console.addCommand(arena, "help", [](auto const &call) {
            context.console.printHelp(call.output);
        });

        if (maybe.isNone()) { return false; }
    }

    // info

    {
        auto maybe = console.addCommand(arena, "info", [](auto const &call) {
            call.output.print("uptime_ms: {}", call.timestamp);
            call.output.print("free_heap: {}", static_cast<kf::u32>(ESP.getFreeHeap()));

            auto const state = context.network.state();
            call.output.print("wifi: {}", service::NetworkService::stateName(state));

            if (state == service::NetworkService::State::Connected) {
                auto const address = context.network.localAddress();
                call.output.print(
                    "ip: {}.{}.{}.{}",
                    (address >> 24) & 0xff,
                    (address >> 16) & 0xff,
                    (address >> 8) & 0xff,
                    address & 0xff);
            }
        });

        if (maybe.isNone()) { return false; }
    }

    // reboot

    {
        auto maybe = console.addCommand(arena, "reboot", [](auto const &call) {
            call.output.print("rebooting");
            call.output.flush();

            // Give the sink a moment to drain before the reset takes effect
            delay(100);
            ESP.restart();
        });

        if (maybe.isNone()) { return false; }
    }

    return true;
}

}// namespace botix::command
