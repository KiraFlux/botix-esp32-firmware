// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/gpio/arduino.hpp>
#include <kf/math/units.hpp>
#include <kf/network/EspNow.hpp>

#include "botix/drivers/actuators/DRV8871.hpp"
#include "botix/drivers/sensors/DualPhaseIncrementalEncoder.hpp"

namespace botix {

using ::kf::network::EspNow;

using MotorDriver = drivers::actuators::DRV8871<::kf::gpio::arduino::PwmOutput>;

using WheelRotaryEncoder = drivers::sensors::DualPhaseIncrementalEncoder<kf::math::Millimeters>;

}// namespace botix