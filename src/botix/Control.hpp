// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Timer.hpp>
#include <kf/math.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>
#include <kf/mixin/TimedPollable.hpp>

#include "botix/Periphery.hpp"

namespace botix {

namespace internal {

struct ControlConfig : kf::mixin::Resettable<ControlConfig> {

    kf::i8
        motor_left_sign,
        motor_right_sign;

private:
    KF_IMPL_RESETTABLE(ControlConfig);
    constexpr void resetImpl() noexcept {
        motor_left_sign = +1;
        motor_right_sign = +1;
    }
};

}// namespace internal

struct Control :

    kf::mixin::NonCopyable,
    kf::mixin::Configured<internal::ControlConfig>,
    kf::mixin::TimedPollable<Control>

{
    using Self = Control;

    using Config = internal::ControlConfig;

    /// @brief Raw data received from the remote controller
    struct Input {
        using ValueType = kf::i16;

        static constexpr ValueType max_value{1000};// MAVLink MANUAL_CONTROL format

        ValueType left_x, left_y, right_x, right_y;
    };

    struct Output : kf::mixin::Resettable<Output> {
        using ValueType = kf::i16;

        ValueType
            motor_left_set{},
            motor_right_set{},
            servo_claw_set{},
            servo_arm_set{};

    private:
        KF_IMPL_RESETTABLE(Output);
        constexpr void resetImpl() noexcept {
            motor_left_set = 0;
            motor_right_set = 0;
            servo_claw_set = 0;
            servo_arm_set = 0;
        }
    };

    using kf::mixin::Configured<Config>::Configured;

    void input(Input const &input) noexcept {
        _got_packet = true;
        // tank mixing
        _output.motor_left_set = (input.left_y + input.left_x) * this->config().motor_left_sign;
        _output.motor_right_set = (input.left_y - input.left_x) * this->config().motor_right_sign;

        _output.servo_claw_set = input.right_x;
        _output.servo_arm_set = input.right_y;
    }

    constexpr Output const &output() const noexcept {
        return _output;
    }

private:
    static constexpr kf::Timer::Config timeout_timer_config{.value = 1000};

    /// @brief Safety timer: if no fresh control packet arrives within 1 s, motors are zeroed
    kf::Timer _timeout_timer{timeout_timer_config};

    Output _output{};
    bool volatile _got_packet{false};

    KF_IMPL_TIMED_POLLABLE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        if (_got_packet) {
            _timeout_timer.start(now);
            _got_packet = false;
        }

        if (_timeout_timer.expired(now)) {
            _output.reset();
        }
    }
};

}// namespace botix