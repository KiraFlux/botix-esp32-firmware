// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/math/units.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

#include <kf/drivers/actuators/DRV8871.hpp>
#include <kf/drivers/actuators/PwmPositionServo.hpp>
#include <kf/drivers/sensors/QuadratureEncoder.hpp>
#include <kf/gpio/ArduinoGPIO.hpp>

namespace botix {

/// @brief Central object that owns and initialises all hardware drivers of the Botix robot
struct Periphery :

    kf::mixin::NonCopyable,
    kf::mixin::Initable<Periphery, bool()>

{
    using GPIO = kf::gpio::ArduinoGPIO;

    /// @brief DRV8871 motor driver bound to the Arduino PWM backend
    using MotorDriver = kf::drivers::actuators::DRV8871<GPIO::PwmOutput>;

    /// @brief MG90S driver with the Arduino PWM backend
    using PwmServo = kf::drivers::actuators::PwmPositionServo<GPIO::PwmOutput>;

    /// @brief Wheel odometry encoder
    /// @details This alias configures the generic QuadratureEncoder to output linear wheel travel in millimeters.
    /// @note The conversion from encoder ticks to millimeters relies on the `units_per_tick` configuration,
    ///       which must reflect the entire kinematic chain (gear ratio, wheel circumference).
    using WheelOdometerEncoder = kf::drivers::sensors::QuadratureEncoder<kf::math::Millimeters>;

    /// @brief Configuration aggregate for all hardware peripherals of the Botix robot
    struct Config : kf::mixin::Resettable<Config> {

        // actuators

        // motor: gpio

        MotorDriver::PwmOutputImpl::Config
            motor_driver_left_pwm_forward,
            motor_driver_left_pwm_backward,
            motor_driver_right_pwm_forward,
            motor_driver_right_pwm_backward;

        MotorDriver::Config motor_driver;

        // servo

        PwmServo::PwmPinImpl::Config
            servo_claw_gpio,
            servo_arm_gpio;

        PwmServo::Config
            servo;

        // sensors

        // wheel odometry

        WheelOdometerEncoder::Config
            wheel_odometry_encoder_left,
            wheel_odometry_encoder_right;

        static constexpr void resetMotorGpio(MotorDriver::PwmOutputImpl::Config &config) noexcept {
            config.frequency_hz = 20000;
            config.resolution_bits = 8;
        }

        static constexpr void resetServoGpio(PwmServo::PwmPinImpl::Config &config) noexcept {
            config.frequency_hz = 50;
            config.resolution_bits = 12;
        }

        /// @brief Default factory settings matching the standard Botix wiring
        [[nodiscard]] static auto defaults() noexcept {
            Config config{};
            config.reset();
            return config;
        }

    private:
        KF_IMPL_RESETTABLE(Config);
        constexpr void resetImpl() noexcept {

            // actuators

            // motors: common

            motor_driver.max_input = 1000;
            motor_driver.forward_dead_zone = 10;
            motor_driver.backward_dead_zone = 10;

            // motors: left

            motor_driver_left_pwm_forward.pin = GPIO_NUM_32;
            motor_driver_left_pwm_forward.channel = 0;
            resetMotorGpio(motor_driver_left_pwm_forward);

            motor_driver_left_pwm_backward.pin = GPIO_NUM_33;
            motor_driver_left_pwm_backward.channel = 1;
            resetMotorGpio(motor_driver_left_pwm_backward);

            // motors: right

            motor_driver_right_pwm_forward.pin = GPIO_NUM_25;
            motor_driver_right_pwm_forward.channel = 2;
            resetMotorGpio(motor_driver_right_pwm_forward);

            motor_driver_right_pwm_backward.pin = GPIO_NUM_26;
            motor_driver_right_pwm_backward.channel = 3;
            resetMotorGpio(motor_driver_right_pwm_backward);

            // servo: common

            servo.angle_range.start = 0;
            servo.angle_range.end = 180;
            servo.pulse_range.start = 500;
            servo.pulse_range.end = 2500;

            // servo: claw

            servo_claw_gpio.pin = GPIO_NUM_13;
            servo_claw_gpio.channel = 4;
            resetServoGpio(servo_claw_gpio);

            // servo: arm

            servo_arm_gpio.pin = GPIO_NUM_14;
            servo_arm_gpio.channel = 5;
            resetServoGpio(servo_arm_gpio);

            // sensors

            // odometry encoder: left

            wheel_odometry_encoder_left.gpio_num_phase_a = GPIO_NUM_36;
            wheel_odometry_encoder_left.gpio_num_phase_b = GPIO_NUM_39;
            wheel_odometry_encoder_left.positive_direction = WheelOdometerEncoder::Config::Direction::CCW;
            wheel_odometry_encoder_left.units_per_tick = 1;

            // odometry encoder: right

            wheel_odometry_encoder_right.gpio_num_phase_a = GPIO_NUM_34;
            wheel_odometry_encoder_right.gpio_num_phase_b = GPIO_NUM_35;
            wheel_odometry_encoder_right.positive_direction = WheelOdometerEncoder::Config::Direction::CW;
            wheel_odometry_encoder_right.units_per_tick = 1;
        }
    };

    explicit Periphery(const Config &config) noexcept :
        config{config} {}

    const Config &config;

    // actuators

    // motors

    MotorDriver motor_driver_left{
        config.motor_driver,
        MotorDriver::PwmOutputImpl{config.motor_driver_left_pwm_forward},
        MotorDriver::PwmOutputImpl{config.motor_driver_left_pwm_backward},
    };

    MotorDriver motor_driver_right{
        config.motor_driver,
        MotorDriver::PwmOutputImpl{config.motor_driver_right_pwm_forward},
        MotorDriver::PwmOutputImpl{config.motor_driver_right_pwm_backward},
    };

    // servos

    PwmServo servo_claw{
        config.servo,
        PwmServo::PwmPinImpl{config.servo_claw_gpio},
    };

    PwmServo servo_arm{
        config.servo,
        PwmServo::PwmPinImpl{config.servo_arm_gpio},
        // safe range
        PwmServo::Config::AngleRange{
            .start = 135,
            .end = 180,
        },
    };

    // sensors

    // odometry encoders

    WheelOdometerEncoder wheel_odometry_encoder_left{
        config.wheel_odometry_encoder_left,
    };

    WheelOdometerEncoder wheel_odometry_encoder_right{
        config.wheel_odometry_encoder_right,
    };

private:
    KF_IMPL_INITABLE(Periphery, bool());
    bool initImpl() noexcept {
        // Encoders must be initialised first because their interrupts start counting immediately
        wheel_odometry_encoder_left.init();
        wheel_odometry_encoder_right.init();

        if (not motor_driver_left.init()) { return false; }
        if (not motor_driver_right.init()) { return false; }

        if (not servo_claw.init()) { return false; }
        if (not servo_arm.init()) { return false; }

        return true;
    }
};

}// namespace botix