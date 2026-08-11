// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/units.hpp>

#include "botix/service/LidarService.hpp"
#include "botix/transport/IpEndpoint.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct LidarSystem : System<LidarSystem, bool()> {

    struct Dependencies {
        service::LidarService::Config const &config;
        /// @brief Where forwarded frames are sent; shared with the UDP transport
        transport::IpEndpoint const &remote;
    };

    explicit constexpr LidarSystem(Dependencies deps) noexcept :
        service{{
            .config = deps.config,
            .remote = deps.remote,
        }} {}

    service::LidarService service;

private:
    BOTIX_IMPL_SYSTEM(LidarSystem, bool());

    bool initImpl() noexcept {
        return service.init();
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        service.poll(now);
    }
};

}// namespace botix::system
