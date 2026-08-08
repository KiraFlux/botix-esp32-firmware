// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/units.hpp>

#include "botix/Periphery.hpp"
#include "botix/service/MixerService.hpp"

#include "botix/behavior/Behavior.hpp"

namespace botix::behavior {

struct OperationalBehavior : Behavior {

    explicit constexpr OperationalBehavior(Periphery &periphery, service::MixerService &mixer_service) noexcept :
        _periphery{periphery}, _mixer_service{mixer_service} {}

    void onQuit() noexcept override {
        _periphery.motor_driver_left.stop();
        _periphery.motor_driver_right.stop();
        _periphery.servo_arm.stop();
        _periphery.servo_claw.stop();
    }

    void onPoll(kf::units::Milliseconds now) noexcept override {
        _mixer_service.poll(now);

        auto const &output = _mixer_service.output();

        _periphery.motor_driver_left.set(output.motor_left_set);
        _periphery.motor_driver_right.set(output.motor_right_set);
        _periphery.servo_arm.set(output.servo_arm_set);
        _periphery.servo_claw.set(output.servo_claw_set);
    }

private:
    Periphery &_periphery;
    service::MixerService &_mixer_service;
};

}// namespace botix::behavior