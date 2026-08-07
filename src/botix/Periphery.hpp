// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/gpio.hpp>
#include <kf/units.hpp>

#include <kf/driver/actuator/DRV8871.hpp>
#include <kf/driver/actuator/PwmPositionServo.hpp>
#include <kf/driver/sensor/QuadratureEncoder.hpp>

#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

namespace botix {

/// @brief Central object that owns and initialises all hardware drivers of the Botix robot
struct Periphery :

    kf::mixin::NonCopyable,
    kf::mixin::Initable<Periphery, bool()>

{
    /// @brief DRV8871 motor driver bound to the Arduino PWM backend
    using MotorDriver = kf::driver::actuator::DRV8871;

    /// @brief MG90S driver with the Arduino PWM backend
    using PwmServo = kf::driver::actuator::PwmPositionServo;

    /// @brief Wheel odometry encoder
    /// @details This alias configures the generic QuadratureEncoder to output linear wheel travel in millimeters.
    /// @note The conversion from encoder ticks to millimeters relies on the `units_per_tick` configuration,
    ///       which must reflect the entire kinematic chain (gear ratio, wheel circumference).
    using WheelOdometerEncoder = kf::driver::sensor::QuadratureEncoder<kf::units::Millimeters>;

    /// @brief Configuration aggregate for all hardware peripherals of the Botix robot
    struct Config : kf::mixin::Resettable<Config> {

        // actuators

        MotorDriver::Config motor_driver;

        PwmServo::Config servo;

        // sensors

        WheelOdometerEncoder::Config
            wheel_odometry_encoder_left,
            wheel_odometry_encoder_right;

    private:
        KF_IMPL_RESETTABLE(Config);
        constexpr void resetImpl() noexcept {

            // actuators

            motor_driver.max_input = 1000;
            motor_driver.duty_dead_zone = 10;
            motor_driver.pwm.frequency_hz = 20000;
            motor_driver.pwm.resolution_bits = 8;

            servo.angle_range.start = 0;
            servo.angle_range.end = 180;
            servo.pulse_range.start = 500;
            servo.pulse_range.end = 2500;
            servo.pwm.frequency_hz = 50,
            servo.pwm.resolution_bits = 12;

            // sensors

            wheel_odometry_encoder_left.units_per_tick = 1;
            wheel_odometry_encoder_left.pull = kf::gpio::DigitalInput::Pull::External;

            wheel_odometry_encoder_right.units_per_tick = 1;
            wheel_odometry_encoder_right.pull = kf::gpio::DigitalInput::Pull::External;
        }
    };

    explicit Periphery(const Config &config) noexcept :
        config{config} {}

    const Config &config;

    // actuators

    // motors

    MotorDriver motor_driver_left{
        config.motor_driver,
        kf::gpio::G32,
        kf::gpio::G33,
    };

    MotorDriver motor_driver_right{
        config.motor_driver,
        kf::gpio::G25,
        kf::gpio::G26,
    };

    // servos

    PwmServo servo_claw{
        config.servo,
        kf::gpio::G13,
    };

    PwmServo servo_arm{
        config.servo,
        kf::gpio::G14,
        // safe range
        PwmServo::Config::AngleRange{
            .start = 135,
            .end = 180,
        },
    };

    // sensors

    // wheel odometry encoders

    WheelOdometerEncoder wheel_odometry_encoder_left{
        config.wheel_odometry_encoder_left,
        // CCW direction
        kf::gpio::G36, kf::gpio::G39,
    };

    WheelOdometerEncoder wheel_odometry_encoder_right{
        config.wheel_odometry_encoder_right,
        // CW direction
        kf::gpio::G35, kf::gpio::G34,
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