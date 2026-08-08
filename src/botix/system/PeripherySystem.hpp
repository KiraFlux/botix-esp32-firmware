// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <WiFi.h>

#include <kf/units.hpp>

#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct PeripherySystem : System<PeripherySystem, void()> {

    constexpr explicit PeripherySystem(RootConfig const &config) noexcept :
        _periphery{config.periphery} {}

    [[nodiscard]] Periphery &periphery() noexcept {
        return _periphery;
    }

private:
    Periphery _periphery;

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

    void pollImpl(kf::units::Milliseconds now) noexcept {}
};

}// namespace botix::system