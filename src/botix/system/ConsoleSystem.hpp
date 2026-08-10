// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/Arena.hpp>
#include <kf/Logger.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/units.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/OutgoingTelemetry.hpp"
#include "botix/command/ConfigCommands.hpp"
#include "botix/command/SystemCommands.hpp"
#include "botix/command/TelemetryCommands.hpp"
#include "botix/command/TransportCommands.hpp"
#include "botix/config/Registry.hpp"
#include "botix/service/ConfigService.hpp"
#include "botix/service/ConsoleService.hpp"
#include "botix/service/NetworkService.hpp"

#include "botix/system/ProtocolSystem.hpp"
#include "botix/system/System.hpp"
#include "botix/system/TransportSystem.hpp"

namespace botix::system {

/// @brief Aggregates every console channel behind one shared command registry
/// @note Channels differ only in where their bytes come from and where output goes;
///       a command issued over the serial line and over MAVLink behaves identically.
struct ConsoleSystem : System<ConsoleSystem, bool()> {

    using Channel = service::ConsoleService::Channel;

    struct Dependencies {
        service::ConsoleService::Config const &config;
        kf::Arena &arena;
        config::Registry &registry;
        service::ConfigService &device_config_service;
        service::ConfigService &user_config_service;
        service::NetworkService &network;
        TransportSystem &transport;
        ProtocolSystem &protocol;
        IncomingTelemetry &incoming_telemetry;
        OutgoingTelemetry &outgoing_telemetry;
        /// @brief Age past which the mixer discards control input
        kf::usize const &max_control_input_age_ms;
    };

    explicit ConsoleSystem(Dependencies deps) noexcept :
        _arena{deps.arena},
        service{deps.config, deps.arena},
        _system_commands{{
            .console = service,
            .network = deps.network,
        }},
        _transport_commands{{
            .transport = deps.transport,
            .protocol = deps.protocol,
        }},
        _config_commands{{
            .registry = deps.registry,
            .device_service = deps.device_config_service,
            .user_service = deps.user_config_service,
        }},
        _telemetry_commands{{
            .incoming = deps.incoming_telemetry,
            .outgoing = deps.outgoing_telemetry,
            .max_control_input_age_ms = deps.max_control_input_age_ms,
        }} {}

    /// @brief Open a channel delivering completed output lines to `sink`
    [[nodiscard]] auto addChannel(auto &&sink) noexcept -> kf::Option<Channel &> {
        auto maybe_channel = service.addChannel(_arena);

        if (maybe_channel.isNone()) {
            _logger.error("channel allocation failed");
            return kf::none;
        }

        auto &channel = maybe_channel.unwrap();
        channel.output.sink(std::forward<decltype(sink)>(sink));

        return kf::someRef(channel);
    }

    service::ConsoleService service;

private:
    kf::Logger _logger{"ConsoleSystem"};

    kf::Arena &_arena;

    command::SystemCommands _system_commands;
    command::TransportCommands _transport_commands;
    command::ConfigCommands _config_commands;
    command::TelemetryCommands _telemetry_commands;

    BOTIX_IMPL_SYSTEM(ConsoleSystem, bool());

    bool initImpl() noexcept {
        if (not _system_commands.registerIn(service, _arena)) {
            _logger.error("system commands registration failed");
            return false;
        }

        if (not _transport_commands.registerIn(service, _arena)) {
            _logger.error("transport commands registration failed");
            return false;
        }

        if (not _config_commands.registerIn(service, _arena)) {
            _logger.error("config commands registration failed");
            return false;
        }

        if (not _telemetry_commands.registerIn(service, _arena)) {
            _logger.error("telemetry commands registration failed");
            return false;
        }

        return true;
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        service.poll(now);
    }
};

}// namespace botix::system
