// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/OutgoingTelemetry.hpp"
#include "botix/service/ConsoleService.hpp"

namespace botix::command {

/// @brief The `telemetry` command: what the robot last received and measured
/// @note Exists so a dead control path can be told apart from a dead drivetrain
///       without guessing: it shows whether commands arrive at all, and how stale
///       the newest one is relative to the mixer's tolerance.
struct TelemetryCommands : kf::mixin::NonCopyable {

    struct Dependencies {
        IncomingTelemetry &incoming;
        OutgoingTelemetry &outgoing;
        /// @brief Age past which the mixer discards control input
        kf::usize const &max_control_input_age_ms;
    };

    explicit constexpr TelemetryCommands(Dependencies deps) noexcept :
        _incoming{deps.incoming},
        _outgoing{deps.outgoing},
        _max_control_input_age_ms{deps.max_control_input_age_ms} {}

    [[nodiscard]] bool registerIn(service::ConsoleService &console, kf::Arena &arena) noexcept {
        return console.addCommand(arena, "telemetry", [this](auto const &call) { report(call); }).isSome();
    }

private:
    using Call = service::ConsoleService::Command::Context;

    IncomingTelemetry &_incoming;
    OutgoingTelemetry &_outgoing;
    kf::usize const &_max_control_input_age_ms;

    void report(Call const &call) const noexcept {
        auto const &input = _incoming.control_input.value();
        auto const age = _incoming.control_input.age(call.timestamp);

        call.output.print("control.x (arm)    : {}", input.x_axis);
        call.output.print("control.y (claw)   : {}", input.y_axis);
        call.output.print("control.z (drive)  : {}", input.z_axis);
        call.output.print("control.r (turn)   : {}", input.r_axis);

        call.output.print(
            "control.age_ms     : {} ({})",
            age,
            age > _max_control_input_age_ms ? "stale, motors held at zero" : "fresh");

        auto const &wheels = _outgoing.wheel_distance.value();
        call.output.print("wheel.left_mm      : {}", wheels.left_mm);
        call.output.print("wheel.right_mm     : {}", wheels.right_mm);
    }
};

}// namespace botix::command
