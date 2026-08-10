// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "botix/OutgoingTelemetry.hpp"
#include "botix/Periphery.hpp"
#include "botix/protocol/Registry.hpp"
#include "botix/service/MixerService.hpp"

#include "botix/config/Config.hpp"

namespace botix::config {

/// @note Bump the version on every layout change, including one nested inside a
///       member config. Size alone does not catch it: this struct is 8-byte
///       aligned, so a new field can hide in trailing padding and let a stale
///       blob load at shifted offsets.
struct DeviceConfig : Config<DeviceConfig, 1> {

    Periphery::Config periphery;

    OutgoingTelemetry::Config outgoing_telemetry;

    protocol::Registry::Config protocol_registry;

    service::MixerService::Config mixer_service;

private:
    KF_IMPL_RESETTABLE(DeviceConfig);
    constexpr void resetImpl() noexcept {
        version = DeviceConfig::latest_version;
        periphery.reset();
        outgoing_telemetry.reset();
        protocol_registry.reset();
        mixer_service.reset();
    }
};

}// namespace botix::config