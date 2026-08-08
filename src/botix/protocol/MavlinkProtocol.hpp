// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <MAVLink.h>

#include <kf/BytesView.hpp>
#include <kf/Timer.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>
#include <kf/mixin/Configured.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Link.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::internal {

/// @brief Configuration for the MAVLink protocol
struct MavlinkProtocolConfig : kf::mixin::Resettable<MavlinkProtocolConfig> {

    /// @brief Timer for HEARTBEAT messages (ms)
    kf::Timer::Config heartbeat_timer;

    kf::u8

        /// @brief MAVLink system ID of this controller
        system_id_self,

        /// @brief MAVLink system ID of the target drone (0 = broadcast)
        system_id_target,

        // components

        component_id_heartbeat,
        component_id_wheel_distance;

private:
    KF_IMPL_RESETTABLE(MavlinkProtocolConfig);
    void resetImpl() noexcept {
        heartbeat_timer.value = 2'000;// ms

        system_id_self = 0x01;
        system_id_target = 0x7f;

        component_id_heartbeat = MAV_COMP_ID_USER1;
        component_id_wheel_distance = MAV_COMP_ID_USER2;
    }
};

}// namespace botix::internal

namespace botix::protocol {

struct MavlinkProtocol :

    Protocol,
    kf::mixin::Configured<internal::MavlinkProtocolConfig>,
    kf::mixin::Callbacked<void(transport::Address const &, mavlink_message_t const &)>

{
    using Config = internal::MavlinkProtocolConfig;

    using kf::mixin::Configured<Config>::Configured;

    [[nodiscard]] static bool sendMessage(transport::Link &transport_link, mavlink_message_t const &message) noexcept {
        kf::u8 buffer[MAVLINK_MAX_PACKET_LEN];
        auto const len = mavlink_msg_to_send_buffer(buffer, &message);

        return transport_link.writeBuffer({buffer, len});
    }

    void poll(PollContext const &context) noexcept override {

        if (context.outgoing_telemetry.wheel_distance.ready(context.timestamp)) {
            (void) sendWheelDistance(context);
        }

        if (_heartbeat_timer.expired(context.timestamp)) {
            _heartbeat_timer.start(context.timestamp);

            (void) sendHeartbeat(context.transport_link);
        }
    }

    void receive(ReceiveContext const &context) noexcept override {
        mavlink_message_t message;
        mavlink_status_t status;

        // separate channels for each transport kind
        auto const channel = static_cast<mavlink_channel_t>(static_cast<int>(context.transport.address.kind()));

        for (auto b: context.transport.buffer) {
            if (mavlink_parse_char(channel, b, &message, &status) != 0) {
                if (not onMessage(context, message)) {
                    this->invoke(context.transport.address, message);
                }
            }
        }
    }

    [[nodiscard]] bool onMessage(ReceiveContext const &context, mavlink_message_t const &message) noexcept {
        switch (message.msgid) {
            case MAVLINK_MSG_ID_MANUAL_CONTROL: {
                mavlink_manual_control_t m;
                mavlink_msg_manual_control_decode(&message, &m);

                context.incoming_telemetry.control_input.update(
                    botix::IncomingTelemetry::ControlInput{
                        .r_axis = m.r,
                        .z_axis = m.z,
                        .y_axis = m.y,
                        .x_axis = m.x,
                    },
                    context.timestamp);

                break;
            }

            default:
                return false;
        }

        return true;
    }

private:
    kf::Timer _heartbeat_timer{this->config().heartbeat_timer};

    [[nodiscard]] bool sendWheelDistance(PollContext const &context) const noexcept {
        mavlink_message_t message;

        kf::u8 const wheel_count = 2;

        kf::f64 const distance[wheel_count]{
            context.outgoing_telemetry.wheel_distance.value().left_mm * 1'000,
            context.outgoing_telemetry.wheel_distance.value().right_mm * 1'000,
        };

        (void) mavlink_msg_wheel_distance_pack(
            this->config().system_id_self,
            this->config().component_id_wheel_distance,
            &message,
            static_cast<kf::u64>(context.timestamp) * 1'000'000,// time_usec
            wheel_count,
            distance);

        return sendMessage(context.transport_link, message);
    }

    [[nodiscard]] bool sendHeartbeat(transport::Link &transport_link) const noexcept {
        mavlink_message_t message;

        (void) mavlink_msg_heartbeat_pack(
            this->config().system_id_self,
            this->config().component_id_heartbeat,
            &message,
            MAV_TYPE_GROUND_ROVER,
            MAV_AUTOPILOT_GENERIC,
            0, 0, 0// Base mode, Custom mode, System status
        );

        return sendMessage(transport_link, message);
    }
};

}// namespace botix::protocol