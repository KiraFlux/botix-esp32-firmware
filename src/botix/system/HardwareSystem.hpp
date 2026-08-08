// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <WiFi.h>

#include <kf/units.hpp>

#include "botix/Periphery.hpp"
#include "botix/config/DeviceConfig.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct HardwareSystem : System<HardwareSystem, void()> {

    struct Dependencies {
        config::DeviceConfig const &config;
    };

    constexpr explicit HardwareSystem(Dependencies deps) noexcept :
        periphery{deps.config.periphery} {}

    Periphery periphery;

private:
    BOTIX_IMPL_SYSTEM(HardwareSystem, void());

    void initImpl() noexcept {
        if (not periphery.init()) {
            // init.logger.error("Periphery init failed");
            return;
        }

        if (not WiFi.mode(WIFI_MODE_STA)) {
            // init.logger.error("WiFi mode failed");
            return;
        }
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {}
};

}// namespace botix::system