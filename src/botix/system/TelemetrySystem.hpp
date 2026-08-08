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

    struct Dependencies {
        RootConfig const &config;
    };

    constexpr explicit TelemetrySystem(Dependencies deps) noexcept :
        outgoing{deps.config.outgoing_telemetry} {}

    IncomingTelemetry incoming{};
    OutgoingTelemetry outgoing;

private:
    BOTIX_IMPL_SYSTEM(TelemetrySystem, void());

    void initImpl() noexcept {}

    void pollImpl(kf::units::Milliseconds now) noexcept {
        outgoing.poll(now);
    }
};

}// namespace botix::system