// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Timer.hpp>
#include <kf/core.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>
#include <kf/mixin/Configured.hpp>
#include <kf/mixin/DefaultResettable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

namespace botix {

namespace internal {

struct OutgoingTelemetryTopicConfig : kf::mixin::DefaultResettable<OutgoingTelemetryTopicConfig> {

    kf::Timer::Config timer{.value = 100};
    kf::u32 update_ahead_ms{10};
    bool enabled{true};
};

struct OutgoingTelemetryConfig : kf::mixin::DefaultResettable<OutgoingTelemetryConfig> {

    OutgoingTelemetryTopicConfig
        wheel_distance{};
};

}// namespace internal

struct OutgoingTelemetry :

    kf::mixin::NonCopyable,
    kf::mixin::TimedPollable<OutgoingTelemetry>,
    kf::mixin::Configured<internal::OutgoingTelemetryConfig>

{
    template<kf::trivial T> struct Topic :

        kf::mixin::NonCopyable,
        kf::mixin::TimedPollable<Topic<T>>,
        kf::mixin::Callbacked<T()>,
        kf::mixin::Configured<internal::OutgoingTelemetryTopicConfig>

    {
        using Config = internal::OutgoingTelemetryTopicConfig;

        using kf::mixin::Configured<Config>::Configured;

        [[nodiscard]] constexpr T const &value() const noexcept {
            return _value;
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

        KF_IMPL_TIMED_POLLABLE(Topic<T>);
        void pollImpl(kf::units::Milliseconds now) noexcept {
            if (not this->config().enabled or (_timer.remaining(now) >= this->config().update_ahead_ms)) {
                return;
            }

            if (auto const maybe_value = this->invoke(); maybe_value.isSome()) {
                _value = maybe_value.unwrap();
            }
        }
    };

    struct WheelDistance {
        kf::f64 left_mm, right_mm;
    };

    using Config = internal::OutgoingTelemetryConfig;

    using kf::mixin::Configured<Config>::Configured;

    Topic<WheelDistance> wheel_distance{this->config().wheel_distance};

private:
    KF_IMPL_TIMED_POLLABLE(OutgoingTelemetry);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        wheel_distance.poll(now);
    }
};

}// namespace botix