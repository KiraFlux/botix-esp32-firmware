// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>

#include <kf/aliases.hpp>
#include <kf/drivers/sensors/Sensor.hpp>
#include <kf/mixin/Configurable.hpp>
#include <kf/mixin/Resettable.hpp>

namespace botix::drivers::sensors {

namespace internal {

/// @brief Dual phase encoder config
/// @tparam T Linear Physical tick value equvalent unit type
template<typename T> struct DualPhaseIncrementalEncoderConfig final : kf::mixin::NonCopyable {

    /// @brief Encoder Tick type
    using TickType = kf::i32;

    /// @brief Encoder Tick type physical linerar equalent
    using UnitType = T;

    /// @brief Encoder tick step type
    using StepType = kf::i8;

    /// @brief Combination of values of A and B phases
    using PhaseStateType = kf::u32;

    /// @brief Rotaty direction
    /// @note Right-arm rule
    enum class Direction : StepType {
        CW = -1,
        CCW = +1,
    };

    UnitType units_per_tick;                  ///< Tick <-> Unit conversion constant
    Direction positive_direction;             ///< Defines which direction will raise tick value
    kf::u8 gpio_num_phase_a, gpio_num_phase_b;///< Phase GPIO numbers

    /// @brief Forward convertion: from ticks to units
    [[nodiscard]] constexpr UnitType unitsFromTicks(TickType value) const noexcept {
        return static_cast<UnitType>(value) * units_per_tick;
    }

    /// @brief Reverse convertion: from units to ticks
    /// @note has division by units_per_tick. Presision might be less, if UnitType is integral type
    [[nodiscard]] constexpr TickType ticksFromUnits(UnitType value) const noexcept {
        return static_cast<TickType>(value / units_per_tick);
    }
};

}// namespace internal

/// @brief Input abstraction for Encoder with Two digital periodic phases
/// @tparam T Linear Physical tick value equvalent unit type
template<typename T> struct DualPhaseIncrementalEncoder final :

    ::kf::drivers::sensors::Sensor<DualPhaseIncrementalEncoder<T>, typename internal::DualPhaseIncrementalEncoderConfig<T>::PhaseStateType, void>,
    ::kf::mixin::Resettable<DualPhaseIncrementalEncoder<T>>,
    ::kf::mixin::Configurable<internal::DualPhaseIncrementalEncoderConfig<T>> {

    using Config = internal::DualPhaseIncrementalEncoderConfig<T>;

    using ::kf::mixin::Configurable<Config>::Configurable;// constructor with just config passing

    // properties

    /// @brief Get current position in ticks
    [[nodiscard]] typename Config::TickType positionTicks() const noexcept { return _position_ticks; }

    /// @brief Set current position in ticks
    void positionTicks(typename Config::TickType position) noexcept { _position_ticks = position; }

    /// @brief Get current position in units
    [[nodiscard]] typename Config::UnitType positionUnits() const noexcept { return this->config().unitsFromTicks(_position_ticks); }

    /// @brief Set current position in units
    void positionUnits(typename Config::UnitType position) noexcept { _position_ticks = this->config().ticksFromUnits(position); }

private:
    // Updated in ISR
    volatile typename Config::TickType _position_ticks{0};  ///< Accumulated steps value
    volatile typename Config::PhaseStateType _last_state{0};///< Last state

    static void IRAM_ATTR onAnyPhaseChange(void *arg) {
        static const typename Config::StepType increment_table[16]{
            //   index: last next : note
            //   base16   AB AB
            0, //  0  :   00 00   : invalid state
            +1,//  1  :   00 01   : rotate     CW
            -1,//  2  :   00 10   : rotate    CCW
            0, //  3  :   00 11   : invalid state
            -1,//  4  :   01 00   : rotate    CCW
            0, //  5  :   01 01   : invalid state
            0, //  6  :   01 10   : invalid state
            +1,//  7  :   01 11   : rotate     CW
            +1,//  8  :   10 00   : rotate     CW
            0, //  9  :   10 01   : invalid state
            0, //  A  :   10 10   : invalid state
            -1,//  B  :   10 11   : rotate    CCW
            0, //  C  :   11 00   : invalid state
            -1,//  D  :   11 01   : rotate    CCW
            +1,//  E  :   11 10   : rotate     CW
            0, //  F  :   11 11   : invalid state
        };

        auto &self{*static_cast<DualPhaseIncrementalEncoder *>(arg)};
        const auto current_state{self.read()};

        self._position_ticks += increment_table[current_state | (self._last_state << 2)] * static_cast<typename Config::StepType>(self.config().positive_direction);
        self._last_state = current_state;
    }

    // impl
    using This = DualPhaseIncrementalEncoder;

    KF_IMPL_INITABLE(This, void);
    void initImpl() noexcept {
        pinMode(this->config().gpio_num_phase_a, INPUT);
        attachInterruptArg(digitalPinToInterrupt(this->config().gpio_num_phase_a), onAnyPhaseChange, static_cast<void *>(this), CHANGE);

        pinMode(this->config().gpio_num_phase_b, INPUT);
        attachInterruptArg(digitalPinToInterrupt(this->config().gpio_num_phase_b), onAnyPhaseChange, static_cast<void *>(this), CHANGE);

        this->reset();
    }

    KF_IMPL(::kf::drivers::sensors::Sensor<DualPhaseIncrementalEncoder<T>, typename Config::PhaseStateType, void>);
    typename Config::PhaseStateType readImpl() const noexcept {
        const auto state_a{static_cast<typename Config::PhaseStateType>(digitalRead(this->config().gpio_num_phase_a))};
        const auto state_b{static_cast<typename Config::PhaseStateType>(digitalRead(this->config().gpio_num_phase_b))};

        return state_b | (state_a << 1);// pack as 0000'0000'0000'0000 ' 0000'0000'0000'00AB
    }

    KF_IMPL_RESETTABLE(This);
    void resetImpl() noexcept {
        _position_ticks = 0;
        _last_state = this->read();
    }
};

}// namespace botix::drivers::sensors