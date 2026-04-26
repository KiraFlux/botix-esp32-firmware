// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/mixin/Configurable.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/prelude.hpp"

namespace botix {

namespace internal {

struct PeripheryConfig final : kf::mixin::NonCopyable {

    MotorDriver::PwmOutputImpl::Config motor_left_pwm_forward, motor_left_pwm_backward, motor_right_pwm_forward, motor_right_pwm_backward;

    MotorDriver::Config motor_driver;

    static constexpr MotorDriver::PwmOutputImpl::Config createMotorPwmOutputConfig(gpio_num_t pin, kf::u8 channel) noexcept {
        return MotorDriver::PwmOutputImpl::Config{
            .frequency_hz = 20000,
            .resolution_bits = 8,
            .pin = static_cast<kf::u8>(pin),
            .channel = channel,
        };
    }

    static constexpr PeripheryConfig defaults() noexcept {
        return PeripheryConfig{
            .motor_left_pwm_forward = createMotorPwmOutputConfig(GPIO_NUM_32, 0),
            .motor_left_pwm_backward = createMotorPwmOutputConfig(GPIO_NUM_33, 1),
            .motor_right_pwm_forward = createMotorPwmOutputConfig(GPIO_NUM_25, 2),
            .motor_right_pwm_backward = createMotorPwmOutputConfig(GPIO_NUM_26, 3),
            .motor_driver = {
                .max_input = 1000,
                .forward_dead_zone = 10,
                .backward_dead_zone = 10,
            },
        };
    }
};

}// namespace internal

struct Periphery final :

    ::kf::mixin::NonCopyable,
    ::kf::mixin::Configurable<internal::PeripheryConfig>,
    ::kf::mixin::Initable<Periphery, bool>

{

    using Config = internal::PeripheryConfig;

    using ::kf::mixin::Configurable<Config>::Configurable;

    MotorDriver motor_driver_left{
        this->config().motor_driver,
        MotorDriver::PwmOutputImpl{this->config().motor_left_pwm_forward},
        MotorDriver::PwmOutputImpl{this->config().motor_left_pwm_backward},
    };

    MotorDriver motor_driver_right{
        this->config().motor_driver,
        MotorDriver::PwmOutputImpl{this->config().motor_right_pwm_forward},
        MotorDriver::PwmOutputImpl{this->config().motor_right_pwm_backward},
    };

private:
    // impl

    using This = Periphery;

    KF_IMPL_INITABLE(This, bool);
    bool initImpl() noexcept {

        if (not motor_driver_left.init()) { return false; }
        if (not motor_driver_right.init()) { return false; }

        return true;
    }
};

}// namespace botix
