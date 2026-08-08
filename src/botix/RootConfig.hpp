// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Resettable.hpp>

#include "botix/OutgoingTelemetry.hpp"
#include "botix/Periphery.hpp"
#include "botix/protocol/Registry.hpp"
#include "botix/service/MixerService.hpp"

namespace botix {

struct RootConfig : kf::mixin::Resettable<RootConfig> {

    Periphery::Config periphery;

    OutgoingTelemetry::Config outgoing_telemetry;

    protocol::Registry::Config protocol_registry;

    service::MixerService::Config mixer_service;

private:
    KF_IMPL_RESETTABLE(RootConfig);
    constexpr void resetImpl() noexcept {
        periphery.reset();
        outgoing_telemetry.reset();
        protocol_registry.reset();
        mixer_service.reset();
    }
};

}// namespace botix