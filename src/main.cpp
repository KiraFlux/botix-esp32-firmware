// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>

#include <kf/Logger.hpp>
#include <kf/math/units.hpp>

#include "botix/Control.hpp"
#include "botix/OperatorTerminal.hpp"
#include "botix/Periphery.hpp"
#include "botix/ui/RootPage.hpp"
#include "botix/ui/UI.hpp"

static auto &ui{botix::ui::UI::instance()};

static auto periphery_config{botix::Periphery::Config::defaults()};

static botix::Periphery periphery{
    periphery_config,
};

static botix::Control control{
    periphery,
};

static botix::OperatorTerminal operator_terminal{
    control,
};

static botix::ui::RootPage root_page{};

void setup() {
    constexpr auto logger{kf::Logger::create("setup")};
    kf::Logger::writer = [](kf::memory::StringView str) { Serial.write(str.data(), str.size()); };

    Serial.begin(115200);

    logger.debug("starting");

    if (not periphery.init()) {
        logger.error("Periphery init failed");
        return;
    }

    periphery.motor_driver_left.stop();
    periphery.motor_driver_right.stop();

    if (not operator_terminal.init()) {
        logger.warn("Operator Terminal init failed");
    }

    ui.bindPage(root_page);

    logger.info("Ready");
}

void loop() {
    constexpr auto loop_period{1000 / 100};// 100 Hz Loop rate

    const auto now{static_cast<kf::math::Milliseconds>(millis())};
    ui.poll(now);
    operator_terminal.poll(now);
    control.poll(now);

    delay(loop_period);
}