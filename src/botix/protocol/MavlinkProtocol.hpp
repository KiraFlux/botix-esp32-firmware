// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <MAVLink.h>

#include <kf/BytesView.hpp>
#include <kf/Timer.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Link.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct MavlinkProtocol :

    Protocol,
    kf::mixin::Callbacked<void(transport::Address const &, mavlink_message_t const &)>

{

    [[nodiscard]] static bool sendMessage(transport::Link &transport_link, mavlink_message_t const &message) noexcept {
        kf::u8 buffer[MAVLINK_MAX_PACKET_LEN];
        auto const len = mavlink_msg_to_send_buffer(buffer, &message);

        return transport_link.writeBuffer({buffer, len});
    }

    void poll(kf::units::Milliseconds now, transport::Link &transport_link) noexcept override {
        // TODO: bulk send telemetry here

        _last_poll = now;

        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);

            (void) sendHeartbeat(transport_link);
        }
    }

    void receive(transport::Address const &address, kf::BytesView buffer, IncomingTelemetry &telemetry) noexcept override {
        mavlink_message_t message;
        mavlink_status_t status;

        // separate channels for each transport kind
        auto const channel = static_cast<mavlink_channel_t>(static_cast<int>(address.kind()));

        for (auto b: buffer) {
            if (mavlink_parse_char(channel, b, &message, &status) != 0) {
                onMessage(address, message, telemetry);
            }
        }
    }

    void onMessage(transport::Address const &address, mavlink_message_t const &message, IncomingTelemetry &telemetry) noexcept {
        switch (message.msgid) {
            case MAVLINK_MSG_ID_MANUAL_CONTROL: {
                mavlink_manual_control_t m;
                mavlink_msg_manual_control_decode(&message, &m);

                telemetry.control_input.update(
                    botix::IncomingTelemetry::ControlInput{
                        .r_axis = m.r,
                        .z_axis = m.z,
                        .y_axis = m.y,
                        .x_axis = m.x,
                    },
                    _last_poll);

                return;
            }

            default:
                this->invoke(address, message);
                return;
        }
    }

private:
    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 1000};

    kf::Timer _heartbeat_timer{heartbeat_timer_config};
    kf::units::Milliseconds _last_poll{};

    [[nodiscard]] bool sendHeartbeat(transport::Link &transport_link) const noexcept {
        mavlink_message_t message;

        (void) mavlink_msg_heartbeat_pack(
            MAV_COMP_ID_USER1,
            MAV_COMP_ID_USER2,
            &message,
            MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_GENERIC,
            0, 0, 0// Base mode, Custom mode, system status
        );

        return sendMessage(transport_link, message);
    }
};

}// namespace botix::protocol