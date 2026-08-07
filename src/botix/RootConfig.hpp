// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Resettable.hpp>

#include "botix/Control.hpp"
#include "botix/Periphery.hpp"

namespace botix {

struct RootConfig : kf::mixin::Resettable<RootConfig> {

    Periphery::Config periphery;
    Control::Config control;

private:
    KF_IMPL_RESETTABLE(RootConfig);
    constexpr void resetImpl() noexcept {
        periphery.reset();
        control.reset();
    }
};

}// namespace botix