// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Logger.hpp>
#include <kf/units.hpp>

#include "botix/Periphery.hpp"
#include "botix/config/DeviceConfig.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct HardwareSystem : System<HardwareSystem, bool()> {

    struct Dependencies {
        config::DeviceConfig const &config;
    };

    constexpr explicit HardwareSystem(Dependencies deps) noexcept :
        periphery{deps.config.periphery} {}

    Periphery periphery;

private:
    kf::Logger _logger{"HardwareSystem"};

    BOTIX_IMPL_SYSTEM(HardwareSystem, bool());

    /// @return false when a driver failed to come up, leaving its actuator inert
    /// @note Reported rather than swallowed: a silent failure here looks exactly
    ///       like a dead control path from the outside, and costs hours to tell apart.
    bool initImpl() noexcept {
        if (not periphery.init()) {
            _logger.error("Periphery init failed: actuators will not respond");
            return false;
        }

        _logger.info("Periphery ready");
        return true;
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {}
};

}// namespace botix::system