// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/gpio/arduino.hpp>
#include <kf/math/units.hpp>

#include "botix/drivers/actuators/DRV8871.hpp"
#include "botix/drivers/sensors/QuadratureEncoder.hpp"

namespace botix {

using MotorDriver = drivers::actuators::DRV8871<::kf::gpio::arduino::PwmOutput>;

/// @brief Wheel odometry encoder
/// @details This alias configures the generic QuadratureEncoder to output linear wheel travel in millimeters.
/// @note The conversion from encoder ticks to millimeters relies on the `units_per_tick` configuration,
///       which must reflect the entire kinematic chain (gear ratio, wheel circumference).
using WheelOdometerEncoder = drivers::sensors::QuadratureEncoder<kf::math::Millimeters>;

}// namespace botix