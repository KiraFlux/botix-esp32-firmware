// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/gpio/arduino.hpp>
#include <kf/network/EspNow.hpp>

#include "botix/drivers/DRV8871.hpp"

namespace botix {

using kf::network::EspNow;

using namespace kf::gpio::arduino;

using DRV8871 = botix::drivers::DRV8871<PwmOutput>;

}// namespace botix
