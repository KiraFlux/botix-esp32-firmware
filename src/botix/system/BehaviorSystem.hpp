// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <MAVLink.h>

#include <kf/units.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/Periphery.hpp"
#include "botix/config/DeviceConfig.hpp"
#include "botix/behavior/Behavior.hpp"
#include "botix/behavior/Kind.hpp"
#include "botix/behavior/Link.hpp"
#include "botix/behavior/Registry.hpp"
#include "botix/service/MixerService.hpp"
#include "botix/transport/Link.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct BehaviorSystem : System<BehaviorSystem, void()> {

    struct Dependencies {
        config::DeviceConfig const &config;
        Periphery &periphery;
        IncomingTelemetry &incoming_telemetry;
    };

    explicit constexpr BehaviorSystem(Dependencies deps) noexcept :
        _mixer_service{deps.config.mixer_service, deps.incoming_telemetry.control_input},
        _registry{deps.periphery, _mixer_service} {}

    [[nodiscard]] auto &get(behavior::Kind kind) noexcept {
        return _registry.get(kind);
    }

private:
    service::MixerService _mixer_service;
    behavior::Registry _registry;

public:
    behavior::Link link{_registry.operational};

private:
    BOTIX_IMPL_SYSTEM(BehaviorSystem, void());

    void initImpl() noexcept {}

    void pollImpl(kf::units::Milliseconds now) noexcept {
        link.onPoll(now);
    }
};

}// namespace botix::system