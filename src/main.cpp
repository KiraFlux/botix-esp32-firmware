// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include <kf/Logger.hpp>
#include <kf/units.hpp>

#include "botix/Control.hpp"
#include "botix/OperatorTerminal.hpp"
#include "botix/Periphery.hpp"

static botix::Periphery::Config periphery_config{};

static botix::Periphery periphery{
    periphery_config,
};

static botix::Control control{
    periphery,
};

static botix::OperatorTerminal operator_terminal{
    control,
};

void kf::main(kf::Init &init) {
    init.logger.debug("starting");

    periphery_config.reset();// set to defaults

    if (not periphery.init()) {
        init.logger.error("Periphery init failed");
        return;
    }

    periphery.motor_driver_left.stop();
    periphery.motor_driver_right.stop();

    if (not operator_terminal.init()) {
        init.logger.warn("Operator Terminal init failed");
    }

    init.logger.info("Ready");

    while (true) {
        constexpr auto loop_period{1000 / 100};// 10 Hz Loop rate

        const auto now = rtos::Clock::now();
        operator_terminal.poll(now);
        control.poll(now);

        rtos::Task::sleep(loop_period);

        init.logger.debug("L: {}, R: {}", periphery.wheel_odometry_encoder_left.positionTicks(), periphery.wheel_odometry_encoder_right.positionTicks());
    }
}
