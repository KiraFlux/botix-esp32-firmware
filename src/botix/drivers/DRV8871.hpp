// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/algorithm.hpp>
#include <kf/drivers/actuators/Actuator.hpp>
#include <kf/gpio/GPIO.hpp>
#include <kf/mixin/Configurable.hpp>

namespace botix::drivers {

namespace internal {

struct DRV8871Config {
    using DutyType = kf::u16;
    using InputType = kf::i16;

    InputType max_input;
    DutyType forward_dead_zone, backward_dead_zone;
};

}// namespace internal

template<typename PwmOutputImpl>
struct DRV8871 final : kf::drivers::actuators::Actuator<DRV8871<PwmOutputImpl>, bool>, kf::mixin::Configurable<internal::DRV8871Config> {
    KF_CHECK_IMPL(PwmOutputImpl, ::kf::gpio::PwmOutputTag);

    using Config = internal::DRV8871Config;

    explicit DRV8871(const Config &config, PwmOutputImpl &&forward, PwmOutputImpl &&backward) noexcept :
        kf::mixin::Configurable<Config>(config),
        _pin_forward{std::move(forward)}, _pin_backward{std::move(backward)} {}

    void set(Config::InputType value) noexcept {
        if (value < 0) {
            _pin_forward.write(0);
            _pin_backward.write(calcDuty(-value, this->config().backward_dead_zone, _pin_backward));
        } else {
            _pin_forward.write(calcDuty(value, this->config().forward_dead_zone, _pin_forward));
            _pin_backward.write(0);
        }
    }

    void stop() noexcept {
        _pin_forward.write(0);
        _pin_backward.write(0);
    }

private:
    PwmOutputImpl _pin_forward, _pin_backward;

    Config::DutyType calcDuty(Config::InputType value, Config::DutyType dead_zone, const PwmOutputImpl &pwm_output) const noexcept {
        return kf::linearMap<Config::DutyType>(kf::clamp(value, static_cast<Config::InputType>(0), this->config().max_input), 0, this->config().max_input, dead_zone, pwm_output.config().maxDuty());
    }

    // impl

    using This = DRV8871<PwmOutputImpl>;

    KF_IMPL_INITABLE(This, bool);
    bool initImpl() noexcept {
        if (not _pin_forward.init()) { return false; }
        if (not _pin_backward.init()) { return false; }

        return true;
    }
};

}// namespace botix::drivers
