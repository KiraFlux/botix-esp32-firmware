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
    volatile int _last_state{0};

    static void IRAM_ATTR isrHandler(void *args) {
        static const Config::StepType increment_table[16]{
            0, //  0
            +1,//  1
            -1,//  2
            0, //  3

            -1,//  4
            0, //  5
            0, //  6
            +1,//  7

            +1,//  8
            0, //  9
            0, // 10
            -1,// 11

            0, // 12
            -1,// 13
            +1,// 14
            0, // 15
        };

        auto &self = *static_cast<DualPhaseIncrementalEncoder *>(args);
        const auto &config = self.config();
        
        const auto current_state{digitalRead(config.pin_phase_b) | (digitalRead(config.pin_phase_a) << 1)};

        self._position += increment_table[current_state | (self._last_state << 2)] * static_cast<Config::StepType>(config.positive_direction);
        self._last_state = current_state;
    }

    // impl
    using This = DualPhaseIncrementalEncoder;

    KF_IMPL_INITABLE(This, void);
    void initImpl() noexcept {
        pinMode(this->config().pin_phase_a, INPUT);
        attachInterruptArg(digitalPinToInterrupt(this->config().pin_phase_a), isrHandler, static_cast<void *>(this), CHANGE);

        pinMode(this->config().pin_phase_b, INPUT);
        attachInterruptArg(digitalPinToInterrupt(this->config().pin_phase_b), isrHandler, static_cast<void *>(this), CHANGE);

        this->reset();
    }

    KF_IMPL_RESETTABLE(This);
    void resetImpl() noexcept {
        _position = 0;
        _last_state = (digitalRead(config().pin_phase_a) << 1) | digitalRead(config().pin_phase_b);
    }
};

}// namespace botix::input