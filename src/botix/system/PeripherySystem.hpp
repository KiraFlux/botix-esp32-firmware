// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <WiFi.h>

#include <kf/units.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"
#include "botix/service/MixerService.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct PeripherySystem : System<PeripherySystem, void()> {

    constexpr explicit PeripherySystem(RootConfig const &config, IncomingTelemetry const &incoming_telemetry) noexcept :
        _periphery{config.periphery}, _mixer_service{config.mixer_service, incoming_telemetry.control_input} {}

    [[nodiscard]] Periphery &periphery() noexcept {
        return _periphery;
    }

private:
    Periphery _periphery;
    service::MixerService _mixer_service;

    BOTIX_IMPL_SYSTEM(PeripherySystem, void());

    void initImpl() noexcept {
        if (not _periphery.init()) {
            // init.logger.error("Periphery init failed");
            return;
        }

        if (not WiFi.mode(WIFI_MODE_STA)) {
            // init.logger.error("WiFi mode failed");
            return;
        }
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        _mixer_service.poll(now);

        auto const &output = _mixer_service.output();

        _periphery.motor_driver_left.set(output.motor_left_set);
        _periphery.motor_driver_right.set(output.motor_right_set);
    }
};

}// namespace botix::system