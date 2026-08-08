// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/NonCopyable.hpp>

#include "botix/Periphery.hpp"
#include "botix/behavior/Behavior.hpp"
#include "botix/behavior/Kind.hpp"
#include "botix/behavior/OperationalBehavior.hpp"
#include "botix/service/MixerService.hpp"

namespace botix::behavior {

struct Registry : kf::mixin::NonCopyable {

    [[nodiscard]] Behavior &get(Kind kind) noexcept {
        (void) kind;
        return operational;
    }

    explicit constexpr Registry(Periphery &periphery, service::MixerService &mixer_service) noexcept :
        operational{periphery, mixer_service} {}

    OperationalBehavior operational;
};

}// namespace botix::behavior