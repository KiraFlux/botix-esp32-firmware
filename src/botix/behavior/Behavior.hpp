// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

namespace botix::behavior {

struct Behavior : kf::mixin::NonCopyable {

    // dynamic interface

    virtual void onEnter() noexcept {}

    virtual void onQuit() noexcept {}

    virtual void onPoll(kf::units::Milliseconds now) noexcept = 0;

    //

    constexpr void requestQuit() noexcept {
        _quit_requested = true;
    }

    [[nodiscard]] constexpr bool quitRequested() const noexcept {
        return _quit_requested;
    }

private:
    bool _quit_requested{false};
};

}// namespace botix::behavior