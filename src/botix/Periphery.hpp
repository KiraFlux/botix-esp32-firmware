// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/mixin/Configurable.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/input/DualPhaseIncrementalEncoder.hpp"
#include "botix/prelude.hpp"

namespace botix {

namespace internal {

struct PeripheryConfig final : kf::mixin::NonCopyable {

    ::botix::PwmOutput::Config motor_left_forward, motor_left_backward, motor_right_forward, motor_right_backward;

    ::botix::DRV8871::Config motor_driver;

    static constexpr PwmOutput::Config createMotorPwmOutputConfig(gpio_num_t pin, kf::u8 channel) noexcept {
        return PwmOutput::Config{
            .frequency_hz = 20000,
            .resolution_bits = 8,
            .pin = static_cast<kf::u8>(pin),
            .channel = channel,
        };
    }

    static constexpr PeripheryConfig defaults() noexcept {
        return PeripheryConfig{
            .motor_left_forward = createMotorPwmOutputConfig(GPIO_NUM_32, 0),
            .motor_left_backward = createMotorPwmOutputConfig(GPIO_NUM_33, 1),
            .motor_right_forward = createMotorPwmOutputConfig(GPIO_NUM_25, 2),
            .motor_right_backward = createMotorPwmOutputConfig(GPIO_NUM_26, 3),
            .motor_driver = {
                .max_input = 1000,
                .forward_dead_zone = 10,
                .backward_dead_zone = 10,
            },
        };
    }
};

}// namespace internal

struct Periphery final : kf::mixin::NonCopyable, kf::mixin::Configurable<internal::PeripheryConfig>, kf::mixin::Initable<Periphery, bool> {

    using Config = internal::PeripheryConfig;

    explicit Periphery(const Config &config) noexcept : kf::mixin::Configurable<Config>{config} {}

    DRV8871 left_motor{
        this->config().motor_driver,
        PwmOutput{this->config().motor_left_forward},
        PwmOutput{this->config().motor_left_backward},
    };

    DRV8871 right_motor{
        this->config().motor_driver,
        PwmOutput{this->config().motor_right_forward},
        PwmOutput{this->config().motor_right_backward},
    };

private:
    // impl

    using This = Periphery;

    KF_IMPL_INITABLE(This, bool);
    bool initImpl() noexcept {

        if (not left_motor.init()) { return false; }
        if (not right_motor.init()) { return false; }

        return true;
    }
};

}// namespace botix
