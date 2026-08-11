// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <MAVLink.h>

#include <kf/BytesView.hpp>
#include <kf/StringView.hpp>
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
        component_id_wheel_distance,
        component_id_serial_control;

private:
    KF_IMPL_RESETTABLE(MavlinkProtocolConfig);
    void resetImpl() noexcept {
        heartbeat_timer.value = 2'000;// ms

        system_id_self = 0x01;
        system_id_target = 0x7f;

        component_id_heartbeat = MAV_COMP_ID_USER1;
        component_id_wheel_distance = MAV_COMP_ID_USER2;
        component_id_serial_control = MAV_COMP_ID_USER3;
    }
};

/// @brief Delivery of console text arriving over `SERIAL_CONTROL`
/// @note Kept as a callback so the protocol layer stays unaware of the console
struct OnSerialControlCallbacked : private kf::mixin::Callbacked<void(kf::StringView)> {

    void onSerialControl(auto &&f) noexcept {
        this->callback(std::forward<decltype(f)>(f));
    }

protected:
    void invokeSerialControlCallback(kf::StringView text) noexcept {
        this->invoke(text);
    }
};

/// @brief Delivery of messages the protocol itself does not handle
struct OnMavlinkFallbackCallbacked : private kf::mixin::Callbacked<void(transport::Address const &, mavlink_message_t const &)> {

    void onMessageFallback(auto &&f) noexcept {
        this->callback(std::forward<decltype(f)>(f));
    }

protected:
    void invokeMessageFallbackCallback(transport::Address const &address, mavlink_message_t const &message) noexcept {
        this->invoke(address, message);
    }
};

}// namespace botix::internal

namespace botix::protocol {

struct MavlinkProtocol :

    Protocol,
    kf::mixin::Configured<internal::MavlinkProtocolConfig>,
    internal::OnSerialControlCallbacked,
    internal::OnMavlinkFallbackCallbacked

{
    using Config = internal::MavlinkProtocolConfig;

    using kf::mixin::Configured<Config>::Configured;

    /// @brief Largest payload a single SERIAL_CONTROL message can carry
    static constexpr kf::usize serial_control_chunk{MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN};

    [[nodiscard]] static bool sendMessage(transport::Link &transport_link, mavlink_message_t const &message) noexcept {
        kf::u8 buffer[MAVLINK_MAX_PACKET_LEN];
        auto const len = mavlink_msg_to_send_buffer(buffer, &message);

        return transport_link.writeBuffer({buffer, len});
    }

    void poll(PollContext const &context) noexcept override {

        if (context.outgoing_telemetry.wheel_distance.ready(context.timestamp)) {
            (void) sendWheelDistance(context);
            (void) sendWheelTicks(context);
        }

        if (_heartbeat_timer.expired(context.timestamp)) {
            _heartbeat_timer.start(context.timestamp);

            (void) sendHeartbeat(context.transport_link);
        }
    }

    /// @brief Send console output back to the operator, split across as many messages as needed
    [[nodiscard]] bool sendSerialControl(transport::Link &transport_link, kf::StringView text) noexcept {
        bool all_sent = true;

        for (kf::usize sent = 0; sent < text.length();) {
            auto const remaining = text.length() - sent;
            auto const count = remaining > serial_control_chunk ? serial_control_chunk : remaining;

            kf::u8 chunk[serial_control_chunk]{};
            for (kf::usize i = 0; i < count; i += 1) {
                chunk[i] = static_cast<kf::u8>(text[sent + i]);
            }

            mavlink_message_t message;

            (void) mavlink_msg_serial_control_pack(
                this->config().system_id_self,
                this->config().component_id_serial_control,
                &message,
                SERIAL_CONTROL_DEV_SHELL,
                SERIAL_CONTROL_FLAG_REPLY,
                0,// timeout
                0,// baudrate: no change
                static_cast<kf::u8>(count),
                chunk,
                this->config().system_id_target,
                MAV_COMP_ID_ALL);

            all_sent = sendMessage(transport_link, message) and all_sent;

            sent += count;
        }

        return all_sent;
    }

    void receive(ReceiveContext const &context) noexcept override {
        mavlink_message_t message;
        mavlink_status_t status;

        // separate channels for each transport kind
        auto const channel = static_cast<mavlink_channel_t>(static_cast<int>(context.transport.address.kind()));

        for (auto b: context.transport.buffer) {
            if (mavlink_parse_char(channel, b, &message, &status) != 0) {
                if (not onMessage(context, message)) {
                    this->invokeMessageFallbackCallback(context.transport.address, message);
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

            case MAVLINK_MSG_ID_SERIAL_CONTROL: {
                mavlink_serial_control_t m;
                mavlink_msg_serial_control_decode(&message, &m);

                // Only the shell device is routed to the console
                if (m.device != SERIAL_CONTROL_DEV_SHELL) {
                    return false;
                }

                // A reply is what this device emits; ignore any echoed back to it
                if ((m.flags & SERIAL_CONTROL_FLAG_REPLY) != 0) {
                    return true;
                }

                auto const count = m.count > serial_control_chunk
                                       ? static_cast<kf::u8>(serial_control_chunk)
                                       : m.count;

                this->invokeSerialControlCallback({
                    reinterpret_cast<char const *>(m.data),
                    count,
                });

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

        // WHEEL_DISTANCE is specified in metres. Multiplying millimetres by 1000
        // overstated it by a factor of a million.
        kf::f64 const distance[wheel_count]{
            context.outgoing_telemetry.wheel_distance.value().left_mm / 1'000,
            context.outgoing_telemetry.wheel_distance.value().right_mm / 1'000,
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

    /// @brief Publish raw encoder counts as a pair of NAMED_VALUE_INT messages
    /// @note No standard message carries two wheel counters, and a custom dialect
    ///       would force every consumer to regenerate headers. Both are stamped
    ///       with the same time_boot_ms so a receiver can pair them.
    [[nodiscard]] bool sendWheelTicks(PollContext const &context) const noexcept {
        auto const &value = context.outgoing_telemetry.wheel_distance.value();
        auto const time_boot_ms = static_cast<kf::u32>(context.timestamp);

        mavlink_message_t message;

        (void) mavlink_msg_named_value_int_pack(
            this->config().system_id_self,
            this->config().component_id_wheel_distance,
            &message,
            time_boot_ms,
            "enc_left",
            value.left_ticks);

        if (not sendMessage(context.transport_link, message)) {
            return false;
        }

        (void) mavlink_msg_named_value_int_pack(
            this->config().system_id_self,
            this->config().component_id_wheel_distance,
            &message,
            time_boot_ms,
            "enc_right",
            value.right_ticks);

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