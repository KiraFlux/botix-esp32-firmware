// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Logger.hpp>
#include <kf/Option.hpp>
#include <kf/math/Timer.hpp>
#include <kf/math/units.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>
#include <kf/network/EspNow.hpp>

#include "botix/Periphery.hpp"

namespace botix {

/// @brief Permanent ESP‑NOW service for interactive robot control and telemetry.
/// @details Lets an operator drive, monitor, and tweak the robot in real time
struct OperatorTerminal final :

    ::kf::mixin::Initable<OperatorTerminal, bool>,
    ::kf::mixin::NonCopyable,
    ::kf::mixin::TimedPollable<OperatorTerminal>

{

    using EspNow = ::kf::network::EspNow;

    /// @brief Sub-service that translates incoming control packets into motor PWM values
    /// @details It implements differential mixing (tank drive) from the four joystick axes,
    ///          updates motors at a fixed rate, and enforces a safety timeout.
    struct Control final : ::kf::mixin::NonCopyable, ::kf::mixin::TimedPollable<Control> {

        /// @brief Raw data received from the remote controller
        struct Input {
            using ValueType = kf::i16;

            ValueType left_x, left_y, right_x, right_y;
        };

        explicit Control(Periphery &periphery) noexcept : _periphery{periphery} {}

        void input(const Input &input) noexcept {
            _got_packet = true;
            // tank mixing
            _motor_left_set = input.left_y + input.left_x;
            _motor_right_set = input.left_y - input.left_x;
        }

    private:
        Periphery &_periphery;

        kf::math::Timer _update_timer{static_cast<kf::math::Milliseconds>(100)};  ///< Timer that fires at 100 Hz to write the latest setpoints to the motor drivers
        kf::math::Timer _timeout_timer{static_cast<kf::math::Milliseconds>(1000)};///< Safety timer: if no fresh control packet arrives within 1 s, motors are zeroed
        Input::ValueType _motor_left_set{}, _motor_right_set{};
        bool _need_reset_update_timer{true};
        volatile bool _got_packet{false};

        // impl
        using This = Control;

        KF_IMPL_TIMED_POLLABLE(This);
        void pollImpl(kf::math::Milliseconds now) noexcept {
            if (_got_packet) {
                _timeout_timer.start(now);
                _got_packet = false;
            }

            if (_timeout_timer.expired(now)) {
                _motor_left_set = 0;
                _motor_right_set = 0;
            }

            if (_update_timer.expired(now) or _need_reset_update_timer) {
                _update_timer.start(now);
                _need_reset_update_timer = false;

                _periphery.motor_driver_left.set(_motor_left_set);
                _periphery.motor_driver_right.set(_motor_right_set);
            }
        }
    };

    explicit OperatorTerminal(Periphery &periphery) noexcept : _control{periphery} {}

private:
    static constexpr auto logger{kf::Logger::create("OperatorTerminal")};

    Control _control;
    kf::Option<EspNow::Peer> _broadcast_peer{};
    kf::math::Timer _heartbeat_timer{static_cast<kf::math::Milliseconds>(1000)};
    bool _need_reset_heartbeat_timer{true};

    // impl
    using This = OperatorTerminal;

    KF_IMPL_INITABLE(This, bool);
    bool initImpl() noexcept {
        auto &espnow{EspNow::instance()};

        const auto init_result{espnow.init()};
        if (init_result.isError()) {
            logger.error("Espnow init failed");
            return false;
        }

        (void) espnow.onReceiveFromUnknown([this](const EspNow::Mac &mac, kf::memory::Slice<const kf::u8> buffer) -> void {
            switch (buffer.size()) {
                case sizeof(Control::Input):
                    _control.input(*reinterpret_cast<const Control::Input *>(buffer.data()));
                    return;

                default:
                    return;
            }
        });

        auto peer_result{EspNow::Peer::add(EspNow::Mac{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})};
        if (peer_result.isOk()) {
            _broadcast_peer.value(std::move(peer_result.value()));
        } else {
            logger.error("Espnow peer add failed");
        }

        return true;
    }

    KF_IMPL_TIMED_POLLABLE(This);
    void pollImpl(kf::math::Milliseconds now) noexcept {
        if (_heartbeat_timer.expired(now) or _need_reset_heartbeat_timer) {
            _heartbeat_timer.start(now);
            _need_reset_heartbeat_timer = false;

            if (_broadcast_peer.hasValue()) {
                (void) _broadcast_peer.value().writeByte(0xAA);
            }

            // Telemetry print that will later be replaced by the UI subsystem
            // Serial.printf("L:\t%d\tR:\t%d\n", int(periphery.wheel_odometry_encoder_left.positionTicks()), int(periphery.wheel_odometry_encoder_right.positionTicks()));
        }

        _control.poll(now);
    }
};

}// namespace botix