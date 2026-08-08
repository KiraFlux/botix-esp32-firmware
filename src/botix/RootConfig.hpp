// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Resettable.hpp>

#include "botix/service/MixerService.hpp"
#include "botix/Periphery.hpp"

namespace botix {

struct RootConfig : kf::mixin::Resettable<RootConfig> {

    Periphery::Config periphery;
    service::MixerService::Config mixer_service;

private:
    KF_IMPL_RESETTABLE(RootConfig);
    constexpr void resetImpl() noexcept {
        periphery.reset();
        mixer_service.reset();
    }
};

}// namespace botix