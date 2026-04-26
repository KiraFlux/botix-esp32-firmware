// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/math/Timer.hpp>
#include <kf/math/units.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

#include "botix/Periphery.hpp"

namespace botix {

/// @brief Sub-service that translates incoming control packets into motor PWM values
/// @details It implements differential mixing (tank drive) from the four joystick axes,
///          updates motors at a fixed rate, and enforces a safety timeout.
struct Control final : kf::mixin::NonCopyable, kf::mixin::TimedPollable<Control> {

    /// @brief Raw data received from the remote controller
    struct Input {
        using ValueType = kf::i16;

        ValueType left_x, left_y, right_x, right_y;
    };

    explicit Control(Periphery &periphery) noexcept : _periphery{periphery} {}

    void input(const Input &input) noexcept {
        _got_packet = true;
        // tank mixing
        _motor_left_set = input.left_y + input.left_x;
        _motor_right_set = input.left_y - input.left_x;
    }

private:
    Periphery &_periphery;

    /// @brief Timer that fires at 100 Hz to write the latest setpoints to the motor drivers
    kf::math::Timer _update_timer{static_cast<kf::math::Milliseconds>(100)};

    /// @brief Safety timer: if no fresh control packet arrives within 1 s, motors are zeroed
    kf::math::Timer _timeout_timer{static_cast<kf::math::Milliseconds>(1000)};

    Input::ValueType _motor_left_set{}, _motor_right_set{};
    bool _need_reset_update_timer{true};
    volatile bool _got_packet{false};

    // impl
    using This = Control;

    KF_IMPL_TIMED_POLLABLE(This);
    void pollImpl(kf::math::Milliseconds now) noexcept {
        if (_got_packet) {
            _timeout_timer.start(now);
            _got_packet = false;
        }

        if (_timeout_timer.expired(now)) {
            _motor_left_set = 0;
            _motor_right_set = 0;

            _periphery.motor_driver_left.stop();
            _periphery.motor_driver_right.stop();
        }

        if (_update_timer.expired(now) or _need_reset_update_timer) {
            _update_timer.start(now);
            _need_reset_update_timer = false;

            _periphery.motor_driver_left.set(_motor_left_set);
            _periphery.motor_driver_right.set(_motor_right_set);
        }
    }
};

}// namespace botix