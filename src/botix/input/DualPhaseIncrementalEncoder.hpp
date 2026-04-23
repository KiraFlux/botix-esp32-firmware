// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>

#include <kf/aliases.hpp>
#include <kf/math/units.hpp>
#include <kf/mixin/Configurable.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

namespace botix::input {

namespace internal {

struct DualPhaseIncrementalEncoderConfig {
    using StepType = kf::i8;
    using TickType = kf::i32;

    enum class Direction : StepType {
        CW = 1,
        CCW = -1,
    };

    kf::math::Millimeters mm_per_tick;
    Direction positive_direction;
    kf::u8 pin_phase_a;
    kf::u8 pin_phase_b;

    constexpr StepType step() const noexcept { return static_cast<kf::math::Millimeters>(positive_direction); }

    [[nodiscard]] constexpr kf::math::Millimeters millimetersFromTicks(TickType ticks) const noexcept {
        return static_cast<kf::math::Millimeters>(ticks) * mm_per_tick;
    }

    [[nodiscard]] constexpr TickType ticksFromMillimeters(kf::math::Millimeters mm) const noexcept {
        return static_cast<TickType>(mm / mm_per_tick);
    }
};

}// namespace internal

struct DualPhaseIncrementalEncoder final
    : kf::mixin::Initable<DualPhaseIncrementalEncoder, void>,
      kf::mixin::NonCopyable,
      kf::mixin::Resettable<DualPhaseIncrementalEncoder>,
      kf::mixin::Configurable<internal::DualPhaseIncrementalEncoderConfig> {

    using Config = internal::DualPhaseIncrementalEncoderConfig;

    using Configurable<Config>::Configurable;

    [[nodiscard]] Config::TickType position() const noexcept { return _position; }

    void position(Config::TickType new_position) noexcept { _position = new_position; }

    [[nodiscard]] kf::math::Millimeters positionMillimeters() const noexcept {
        return this->config().millimetersFromTicks(position());
    }

    void positionMillimeters(kf::math::Millimeters new_position_mm) noexcept {
        _position = this->config().ticksFromMillimeters(new_position_mm);
    }

private:
    volatile Config::TickType _position{0};

    static void IRAM_ATTR isrHandler(void *args) {
        auto &self = *static_cast<DualPhaseIncrementalEncoder *>(args);

        if (digitalRead(self.config().pin_phase_b)) {
            self._position += self.config().step();
        } else {
            self._position -= self.config().step();
        }
    }

    // impl
    using This = DualPhaseIncrementalEncoder;

    KF_IMPL_INITABLE(This, void);
    void initImpl() noexcept {
        pinMode(this->config().pin_phase_a, INPUT);
        pinMode(this->config().pin_phase_b, INPUT);

        attachInterruptArg(digitalPinToInterrupt(this->config().pin_phase_a), isrHandler, static_cast<void *>(this), RISING);
    }

    KF_IMPL_RESETTABLE(This);
    void resetImpl() noexcept {
        _position = 0;
    }
};

}// namespace botix::input