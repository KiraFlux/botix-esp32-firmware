// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Configurable.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/prelude.hpp"

namespace botix {

namespace internal {

/// @brief Configuration aggregate for all hardware peripherals of the Botix robot
struct PeripheryConfig final : kf::mixin::NonCopyable {

    /// @brief PWM channel configurations for the two half‑bridges of each motor
    MotorDriver::PwmOutputImpl::Config
        motor_driver_left_pwm_forward,
        motor_driver_left_pwm_backward,
        motor_driver_right_pwm_forward,
        motor_driver_right_pwm_backward;

    /// @brief Common motor driver parameters (dead zones, max input)
    MotorDriver::Config motor_driver;

    /// @brief Configurations for left and right quadrature encoders
    WheelOdometerEncoder::Config
        wheel_odometry_encoder_left,
        wheel_odometry_encoder_right;

    /// @brief Helper to create a PWM output configuration for a given GPIO and LEDC channel
    static constexpr MotorDriver::PwmOutputImpl::Config defaultPwmOutputConfig(gpio_num_t pin, kf::u8 channel) noexcept {
        return MotorDriver::PwmOutputImpl::Config{
            .frequency_hz = 20000,
            .resolution_bits = 8,
            .pin = static_cast<kf::u8>(pin),
            .channel = channel,
        };
    }

    /// @brief Helper to create an encoder configuration for a pair of GPIOs
    static constexpr WheelOdometerEncoder::Config defaultWheelOdometerEncoderConfig(
        gpio_num_t a, gpio_num_t b,
        WheelOdometerEncoder::Config::Direction positive_direction) noexcept {
        return WheelOdometerEncoder::Config{
            .units_per_tick = static_cast<WheelOdometerEncoder::Config::UnitType>(1),// placeholder, real value depends on kinematic chain
            .positive_direction = positive_direction,
            .gpio_num_phase_a = static_cast<WheelOdometerEncoder::Config::GpioNumType>(a),
            .gpio_num_phase_b = static_cast<WheelOdometerEncoder::Config::GpioNumType>(b),
        };
    }

    /// @brief Default motor driver parameters (dead zones, max input)
    static constexpr MotorDriver::Config defaultMotorDriverConfig() noexcept {
        return MotorDriver::Config{
            .max_input = 1000,
            .forward_dead_zone = 10,
            .backward_dead_zone = 10,
        };
    }

    /// @brief Default factory settings matching the standard Botix wiring
    static constexpr PeripheryConfig defaults() noexcept {
        return PeripheryConfig{
            .motor_driver_left_pwm_forward = defaultPwmOutputConfig(GPIO_NUM_32, 0),
            .motor_driver_left_pwm_backward = defaultPwmOutputConfig(GPIO_NUM_33, 1),
            .motor_driver_right_pwm_forward = defaultPwmOutputConfig(GPIO_NUM_25, 2),
            .motor_driver_right_pwm_backward = defaultPwmOutputConfig(GPIO_NUM_26, 3),
            .motor_driver = defaultMotorDriverConfig(),
            .wheel_odometry_encoder_left = defaultWheelOdometerEncoderConfig(GPIO_NUM_36, GPIO_NUM_39, WheelOdometerEncoder::Config::Direction::CCW),
            .wheel_odometry_encoder_right = defaultWheelOdometerEncoderConfig(GPIO_NUM_34, GPIO_NUM_35, WheelOdometerEncoder::Config::Direction::CW),
        };
    }
};

}// namespace internal

/// @brief Central object that owns and initialises all hardware drivers of the Botix robot
struct Periphery final :

    ::kf::mixin::NonCopyable,
    ::kf::mixin::Configurable<internal::PeripheryConfig>,
    ::kf::mixin::Initable<Periphery, bool>

{
    using Config = internal::PeripheryConfig;

    using ::kf::mixin::Configurable<Config>::Configurable;

    MotorDriver motor_driver_left{
        this->config().motor_driver,
        MotorDriver::PwmOutputImpl{this->config().motor_driver_left_pwm_forward},
        MotorDriver::PwmOutputImpl{this->config().motor_driver_left_pwm_backward},
    };

    MotorDriver motor_driver_right{
        this->config().motor_driver,
        MotorDriver::PwmOutputImpl{this->config().motor_driver_right_pwm_forward},
        MotorDriver::PwmOutputImpl{this->config().motor_driver_right_pwm_backward},
    };

    WheelOdometerEncoder wheel_odometry_encoder_left{
        this->config().wheel_odometry_encoder_left,
    };

    WheelOdometerEncoder wheel_odometry_encoder_right{
        this->config().wheel_odometry_encoder_right,
    };

private:
    // impl
    using This = Periphery;

    KF_IMPL_INITABLE(This, bool);
    bool initImpl() noexcept {
        // Encoders must be initialised first because their interrupts start counting immediately
        wheel_odometry_encoder_left.init();
        wheel_odometry_encoder_right.init();

        if (not motor_driver_left.init()) { return false; }
        if (not motor_driver_right.init()) { return false; }

        return true;
    }
};

}// namespace botix