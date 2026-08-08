// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/units.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/OutgoingTelemetry.hpp"
#include "botix/RootConfig.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct TelemetrySystem : System<TelemetrySystem, void()> {

    constexpr explicit TelemetrySystem(RootConfig const &config) noexcept :
        _outgoing{config.outgoing_telemetry} {}

    [[nodiscard]] IncomingTelemetry &incoming() noexcept {
        return _incoming;
    }

    [[nodiscard]] OutgoingTelemetry &outgoing() noexcept {
        return _outgoing;
    }

private:
    IncomingTelemetry _incoming;
    OutgoingTelemetry _outgoing;

    BOTIX_IMPL_SYSTEM(TelemetrySystem, void());

    void initImpl() noexcept {}

    void pollImpl(kf::units::Milliseconds now) noexcept {
        _outgoing.poll(now);
    }
};

}// namespace botix::system