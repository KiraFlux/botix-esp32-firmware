// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/behavior/Behavior.hpp"

namespace botix::behavior {

struct Link : kf::mixin::NonCopyable {

    explicit constexpr Link(Behavior &default_behavior) noexcept :
        _default_behavior{default_behavior} {}

    void set(Behavior &new_behavior) noexcept {
        _current_behavior->onQuit();
        new_behavior.onEnter();
        _current_behavior = &new_behavior;
    }

    void onEnter() noexcept {
        _current_behavior->onEnter();
    }

    void onQuit() noexcept {
        _current_behavior->onQuit();
    }

    void onPoll(kf::units::Milliseconds now) noexcept {
        _current_behavior->onPoll(now);

        if (_current_behavior->quitRequested()) {
            set(_default_behavior);
        }
    }

private:
    Behavior &_default_behavior;
    Behavior *_current_behavior{&_default_behavior};
};

}// namespace botix::behavior