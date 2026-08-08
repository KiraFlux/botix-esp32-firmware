// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <MAVLink.h>

#include <kf/units.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"
#include "botix/behavior/Behavior.hpp"
#include "botix/behavior/Kind.hpp"
#include "botix/behavior/Link.hpp"
#include "botix/behavior/Registry.hpp"
#include "botix/service/MixerService.hpp"
#include "botix/transport/Link.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct BehaviorSystem : System<BehaviorSystem, void()> {

    explicit constexpr BehaviorSystem(RootConfig const &config, Periphery &periphery, IncomingTelemetry &incoming_telemetry) noexcept :
        _mixer_service{config.mixer_service, incoming_telemetry.control_input},
        _registry{periphery, _mixer_service} {}

    [[nodiscard]] behavior::Link &link() noexcept {
        return _link;
    }

    [[nodiscard]] behavior::Behavior &get(behavior::Kind kind) noexcept {
        return _registry.get(kind);
    }

private:
    service::MixerService _mixer_service;
    behavior::Registry _registry;
    behavior::Link _link{_registry.operational};

    BOTIX_IMPL_SYSTEM(BehaviorSystem, void());

    void initImpl() noexcept {}

    void pollImpl(kf::units::Milliseconds now) noexcept {
        _link.onPoll(now);
    }
};

}// namespace botix::system