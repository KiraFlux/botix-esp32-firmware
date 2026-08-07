// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <MAVLink.h>

#include <kf/Slice.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>

#include "botix/transport/TransportLink.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct MavlinkProtocol : Protocol, kf::mixin::Callbacked<void(mavlink_message_t const &)> {

    [[nodiscard]] static bool sendMessage(transport::TransportLink &transport_link, mavlink_message_t const &message) noexcept {
        kf::u8 buffer[MAVLINK_MAX_PACKET_LEN];
        auto const len = mavlink_msg_to_send_buffer(buffer, &message);

        return transport_link.sendBuffer({buffer, len});
    }

    void poll(kf::units::Milliseconds now, transport::TransportLink &transport_link) noexcept override {
        // TODO: bulk send telemetry here

        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);
            
            (void) sendHeartbeat(transport_link);
        }
    }

    void receive(kf::Slice<kf::u8 const> buffer) noexcept override {
        mavlink_message_t message;
        mavlink_status_t status;

        for (auto b: buffer) {
            if (mavlink_parse_char(MAVLINK_COMM_0, b, &message, &status) != 0) {
                this->invoke(message);
            }
        }
    }

private:
    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 1000};

    kf::Timer _heartbeat_timer{heartbeat_timer_config};

    [[nodiscard]] bool sendHeartbeat(transport::TransportLink &transport_link) const noexcept {
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