// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Timer.hpp>
#include <kf/concepts.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

namespace botix {

namespace internal {

struct OutgoingTelemetryTopicConfig : kf::mixin::Resettable<OutgoingTelemetryTopicConfig> {

    kf::Timer::Config timer;
    kf::u32 update_ahead_ms;
    bool enabled;

private:
    KF_IMPL_RESETTABLE(OutgoingTelemetryTopicConfig);
    constexpr void resetImpl() noexcept {
        timer.value = 100;
        update_ahead_ms = 10;
        enabled = true;
    }
};

struct OutgoingTelemetryConfig : kf::mixin::Resettable<OutgoingTelemetryConfig> {

    OutgoingTelemetryTopicConfig
        wheel_distance;

private:
    KF_IMPL_RESETTABLE(OutgoingTelemetryConfig);
    constexpr void resetImpl() noexcept {
        wheel_distance.reset();
    }
};

}// namespace internal

struct OutgoingTelemetry :

    kf::mixin::NonCopyable,
    kf::mixin::Configured<internal::OutgoingTelemetryConfig>

{
    using Config = internal::OutgoingTelemetryConfig;

    template<kf::trivial T> struct Topic :

        kf::mixin::NonCopyable,
        kf::mixin::Configured<internal::OutgoingTelemetryTopicConfig>

    {
        using Config = internal::OutgoingTelemetryTopicConfig;

        using kf::mixin::Configured<Config>::Configured;

        constexpr void value(T const &new_value) noexcept {
            _value = new_value;
        }

        [[nodiscard]] constexpr T const &value() const noexcept {
            return _value;
        }

        [[nodiscard]] bool shouldUpdate(kf::units::Milliseconds now) const noexcept {
            return this->config().enabled and (_timer.remaining(now) < this->config().update_ahead_ms);
        }

        [[nodiscard]] bool ready(kf::units::Milliseconds now) noexcept {
            if (not this->config().enabled) {
                return false;
            }

            if (_timer.expired(now)) {
                _timer.start(now);
                return true;
            }

            return false;
        }

    private:
        kf::Timer _timer{this->config().timer};
        T _value{};
    };

    struct WheelDistance {
        kf::f64 left_mm, right_mm;
    };

    using kf::mixin::Configured<Config>::Configured;

    Topic<WheelDistance> wheel_distance{this->config().wheel_distance};
};

}// namespace botix