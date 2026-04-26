// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Logger.hpp>
#include <kf/Option.hpp>
#include <kf/math/Timer.hpp>
#include <kf/math/units.hpp>
#include <kf/memory/Array.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>
#include <kf/network/EspNow.hpp>

#include "botix/Control.hpp"
#include "botix/ui/UI.hpp"

namespace botix {

/// @brief Permanent ESP‑NOW service for interactive robot control and telemetry.
/// @details Lets an operator drive, monitor, and tweak the robot in real time
struct OperatorTerminal final :

    ::kf::mixin::Initable<OperatorTerminal, bool>,
    ::kf::mixin::NonCopyable,
    ::kf::mixin::TimedPollable<OperatorTerminal>

{
    explicit OperatorTerminal(Control &control) noexcept : _control{control} {}

private:
    using EspNow = ::kf::network::EspNow;

    static constexpr auto logger{kf::Logger::create("OperatorTerminal")};

    Control &_control;
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

                case sizeof(ui::UI::Event):
                    ui::UI::instance().addEvent(*reinterpret_cast<const ui::UI::Event *>(buffer.data()));
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

        auto &render_config{ui::UI::instance().renderConfig()};
        render_config.title_centered = false;
        render_config.row_max_length = 20;
        render_config.rows_total = 8;
        render_config.callback([this](kf::memory::StringView str) -> void {
            if (_broadcast_peer.hasValue()) {
                (void) _broadcast_peer.value().writeBuffer({reinterpret_cast<const kf::u8 *>(str.data()), str.size()});
            }
        });

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
    }
};

}// namespace botix