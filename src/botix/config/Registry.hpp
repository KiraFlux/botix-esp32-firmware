// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/Slice.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include "botix/config/DeviceConfig.hpp"
#include "botix/config/EnumOption.hpp"
#include "botix/config/Field.hpp"
#include "botix/config/Kind.hpp"
#include "botix/config/Section.hpp"
#include "botix/config/UserConfig.hpp"

/// @brief Byte offset of a possibly nested member
/// @note The config structs carry empty CRTP bases, so they are not standard layout
///       and `offsetof` is only conditionally supported. GCC computes it correctly
///       for these aggregates; the diagnostic is suppressed around the tables below.
#define BOTIX_CONFIG_OFFSET(__type__, ...) \
    static_cast<kf::u16>(offsetof(__type__, __VA_ARGS__))

namespace botix::config {

/// @brief Name-addressable view over every persisted configuration field
struct Registry {

    /// @brief Number of sections; kept as a constant so the array needs no allocation
    static constexpr kf::usize section_count{2};

    explicit Registry(DeviceConfig &device, UserConfig &user) noexcept;

    [[nodiscard]] constexpr auto sections() const noexcept -> kf::Slice<Section const> {
        return {_sections, section_count};
    }

    [[nodiscard]] constexpr auto findSection(kf::StringView name) const noexcept -> kf::Option<Section const &> {
        for (auto const &section: _sections) {
            if (section.name == name) {
                return kf::someRef(section);
            }
        }
        return kf::none;
    }

    /// @brief Resolve a fully qualified `section.path` reference
    struct Resolved {
        Section const &section;
        Field const &field;
    };

    [[nodiscard]] constexpr auto resolve(kf::StringView qualified) const noexcept -> kf::Option<Resolved> {
        auto const separator = qualified.indexOf('.');
        if (separator.isNone()) {
            return kf::none;
        }

        auto const section_name = qualified.first(separator.unwrap());
        auto const field_path = qualified.fromOffset(separator.unwrap() + 1);

        auto const maybe_section = findSection(section_name);
        if (maybe_section.isNone()) {
            return kf::none;
        }

        auto const &section = maybe_section.unwrap();

        auto const maybe_field = section.find(field_path);
        if (maybe_field.isNone()) {
            return kf::none;
        }

        return kf::some(Resolved{
            .section = section,
            .field = maybe_field.unwrap(),
        });
    }

private:
    Section _sections[section_count];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

/// @brief Declare a scalar field of a config struct
#define BOTIX_FIELD(__config__, __path__, __member__, __kind__, __type__) \
    Field {                                                               \
        .path = __path__,                                                 \
        .offset = BOTIX_CONFIG_OFFSET(__config__, __member__),            \
        .size = static_cast<kf::u16>(sizeof(__type__)),                   \
        .kind = __kind__,                                                 \
        .secret = false,                                                  \
        .options = {},                                                    \
        .min = kf::none,                                                  \
        .max = kf::none,                                                  \
    }

    static constexpr EnumOption encoder_pull_options[]{
        {"external", static_cast<kf::usize>(kf::gpio::DigitalInput::Pull::External)},
        {"pulldown", static_cast<kf::usize>(kf::gpio::DigitalInput::Pull::InternalDown)},
        {"pullup", static_cast<kf::usize>(kf::gpio::DigitalInput::Pull::InternalUp)},
    };

    static constexpr EnumOption mixer_mode_options[]{
        {"direct", static_cast<kf::usize>(botix::internal::MixerServiceConfig::Mode::Direct)},
        {"tank", static_cast<kf::usize>(botix::internal::MixerServiceConfig::Mode::Tank)},
    };

    static constexpr EnumOption transport_kind_options[]{
        {"espnow", static_cast<kf::usize>(transport::Kind::Espnow)},
        {"wifi", static_cast<kf::usize>(transport::Kind::Wifi)},
    };

    static constexpr EnumOption protocol_kind_options[]{
        {"raw", static_cast<kf::usize>(protocol::Kind::Raw)},
        {"mavlink", static_cast<kf::usize>(protocol::Kind::Mavlink)},
    };

    /// @brief Every addressable field of `DeviceConfig`
    /// @note `version` is deliberately absent: it is owned by the migration logic
    static constexpr Field device_fields[]{
        // periphery / motors
        BOTIX_FIELD(DeviceConfig, "motor.max_input", periphery.motor_driver.max_input, Kind::Signed, kf::i16),
        BOTIX_FIELD(DeviceConfig, "motor.dead_zone", periphery.motor_driver.duty_dead_zone, Kind::Unsigned, kf::u16),
        BOTIX_FIELD(DeviceConfig, "motor.pwm_hz", periphery.motor_driver.pwm.frequency_hz, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "motor.pwm_bits", periphery.motor_driver.pwm.resolution_bits, Kind::Unsigned, kf::u8),

        // periphery / servos
        BOTIX_FIELD(DeviceConfig, "servo.angle_min", periphery.servo.angle_range.start, Kind::Unsigned, kf::u16),
        BOTIX_FIELD(DeviceConfig, "servo.angle_max", periphery.servo.angle_range.end, Kind::Unsigned, kf::u16),
        BOTIX_FIELD(DeviceConfig, "servo.pulse_min", periphery.servo.pulse_range.start, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "servo.pulse_max", periphery.servo.pulse_range.end, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "servo.pwm_hz", periphery.servo.pwm.frequency_hz, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "servo.pwm_bits", periphery.servo.pwm.resolution_bits, Kind::Unsigned, kf::u8),

        // periphery / encoders
        BOTIX_FIELD(DeviceConfig, "encoder.left_mm_per_tick", periphery.wheel_odometry_encoder_left.units_per_tick, Kind::Real, kf::f64),
        BOTIX_FIELD(DeviceConfig, "encoder.right_mm_per_tick", periphery.wheel_odometry_encoder_right.units_per_tick, Kind::Real, kf::f64),
        Field{
            .path = "encoder.pull",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, periphery.wheel_odometry_encoder_left.pull),
            .size = sizeof(kf::gpio::DigitalInput::Pull),
            .kind = Kind::Enumerated,
            .secret = false,
            .options = {encoder_pull_options},
            .min = kf::none,
            .max = kf::none,
        },
        Field{
            .path = "encoder.right_pull",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, periphery.wheel_odometry_encoder_right.pull),
            .size = sizeof(kf::gpio::DigitalInput::Pull),
            .kind = Kind::Enumerated,
            .secret = false,
            .options = {encoder_pull_options},
            .min = kf::none,
            .max = kf::none,
        },

        // telemetry
        BOTIX_FIELD(DeviceConfig, "telemetry.wheel.period_ms", outgoing_telemetry.wheel_distance.timer.value, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "telemetry.wheel.ahead_ms", outgoing_telemetry.wheel_distance.update_ahead_ms, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "telemetry.wheel.enabled", outgoing_telemetry.wheel_distance.enabled, Kind::Boolean, bool),

        // mavlink protocol
        BOTIX_FIELD(DeviceConfig, "mavlink.heartbeat_ms", protocol_registry.mavlink.heartbeat_timer.value, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(DeviceConfig, "mavlink.sysid_self", protocol_registry.mavlink.system_id_self, Kind::Unsigned, kf::u8),
        BOTIX_FIELD(DeviceConfig, "mavlink.sysid_target", protocol_registry.mavlink.system_id_target, Kind::Unsigned, kf::u8),
        BOTIX_FIELD(DeviceConfig, "mavlink.compid_heartbeat", protocol_registry.mavlink.component_id_heartbeat, Kind::Unsigned, kf::u8),
        BOTIX_FIELD(DeviceConfig, "mavlink.compid_wheel", protocol_registry.mavlink.component_id_wheel_distance, Kind::Unsigned, kf::u8),
        BOTIX_FIELD(DeviceConfig, "mavlink.compid_serial", protocol_registry.mavlink.component_id_serial_control, Kind::Unsigned, kf::u8),

        // mixer
        BOTIX_FIELD(DeviceConfig, "mixer.max_input_age_ms", mixer_service.max_control_input_age_ms, Kind::Unsigned, kf::usize),
        Field{
            .path = "mixer.mode",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.mode),
            .size = sizeof(botix::internal::MixerServiceConfig::Mode),
            .kind = Kind::Enumerated,
            .secret = false,
            .options = {mixer_mode_options},
            .min = kf::none,
            .max = kf::none,
        },
        Field{
            .path = "mixer.left_sign",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.motor_left_sign),
            .size = sizeof(kf::i8),
            .kind = Kind::Signed,
            .secret = false,
            .options = {},
            .min = kf::some(-1.0),
            .max = kf::some(1.0),
        },
        Field{
            .path = "mixer.left_scale",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.motor_left_scale),
            .size = sizeof(kf::u16),
            .kind = Kind::Unsigned,
            .secret = false,
            .options = {},
            .min = kf::some(0.0),
            .max = kf::some(1000.0),
        },
        Field{
            .path = "mixer.right_scale",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.motor_right_scale),
            .size = sizeof(kf::u16),
            .kind = Kind::Unsigned,
            .secret = false,
            .options = {},
            .min = kf::some(0.0),
            .max = kf::some(1000.0),
        },
        Field{
            .path = "mixer.right_sign",
            .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.motor_right_sign),
            .size = sizeof(kf::i8),
            .kind = Kind::Signed,
            .secret = false,
            .options = {},
            .min = kf::some(-1.0),
            .max = kf::some(1.0),
        },
    };

    /// @brief Every addressable field of `UserConfig`
    static constexpr Field user_fields[]{
        Field{
            .path = "boot.transport",
            .offset = BOTIX_CONFIG_OFFSET(UserConfig, init_transport_kind),
            .size = sizeof(transport::Kind),
            .kind = Kind::Enumerated,
            .secret = false,
            .options = {transport_kind_options},
            .min = kf::none,
            .max = kf::none,
        },
        Field{
            .path = "boot.protocol",
            .offset = BOTIX_CONFIG_OFFSET(UserConfig, init_protocol_kind),
            .size = sizeof(protocol::Kind),
            .kind = Kind::Enumerated,
            .secret = false,
            .options = {protocol_kind_options},
            .min = kf::none,
            .max = kf::none,
        },

        // wifi station
        BOTIX_FIELD(UserConfig, "wifi.enabled", network.enabled, Kind::Boolean, bool),
        BOTIX_FIELD(UserConfig, "wifi.ssid", network.ssid, Kind::Text, decltype(UserConfig::network.ssid)),
        Field{
            .path = "wifi.password",
            .offset = BOTIX_CONFIG_OFFSET(UserConfig, network.password),
            .size = static_cast<kf::u16>(sizeof(decltype(UserConfig::network.password))),
            .kind = Kind::Text,
            .secret = true,
            .options = {},
            .min = kf::none,
            .max = kf::none,
        },
        BOTIX_FIELD(UserConfig, "wifi.hostname", network.hostname, Kind::Text, decltype(UserConfig::network.hostname)),
        BOTIX_FIELD(UserConfig, "wifi.retry_ms", network.retry_timer.value, Kind::Unsigned, kf::u32),

        // udp transport
        BOTIX_FIELD(UserConfig, "udp.local_port", transport_registry.wifi_udp.local_port, Kind::Unsigned, kf::u16),
        BOTIX_FIELD(UserConfig, "udp.remote_ip", transport_registry.wifi_udp.remote.address, Kind::Ipv4, kf::u32),
        // lidar
        BOTIX_FIELD(UserConfig, "lidar.enabled", lidar.enabled, Kind::Boolean, bool),
        BOTIX_FIELD(UserConfig, "lidar.baudrate", lidar.baudrate, Kind::Unsigned, kf::u32),
        BOTIX_FIELD(UserConfig, "lidar.rx_buffer", lidar.rx_buffer_length, Kind::Unsigned, kf::u16),
        Field{
            .path = "lidar.uart",
            .offset = BOTIX_CONFIG_OFFSET(UserConfig, lidar.uart_num),
            .size = sizeof(kf::u8),
            .kind = Kind::Unsigned,
            .secret = false,
            .options = {},
            // UART0 is the console and the programming line
            .min = kf::some(1.0),
            .max = kf::some(2.0),
        },
        BOTIX_FIELD(UserConfig, "lidar.port", lidar.remote_port, Kind::Unsigned, kf::u16),
        Field{
            .path = "lidar.inverted",
            .offset = BOTIX_CONFIG_OFFSET(UserConfig, lidar.inverted),
            .size = sizeof(kf::u8),
            .kind = Kind::Unsigned,
            .secret = false,
            .options = {},
            .min = kf::some(0.0),
            .max = kf::some(1.0),
        },
        Field{
            .path = "lidar.rx_pin",
            .offset = BOTIX_CONFIG_OFFSET(UserConfig, lidar.rx_pin),
            .size = sizeof(kf::u8),
            .kind = Kind::Unsigned,
            .secret = false,
            .options = {},
            .min = kf::some(1.0),
            .max = kf::some(39.0),
        },

        BOTIX_FIELD(UserConfig, "udp.remote_port", transport_registry.wifi_udp.remote.port, Kind::Unsigned, kf::u16),
    };

#pragma GCC diagnostic pop

#undef BOTIX_FIELD

    // A stale offset would silently read and write a neighbouring field, and a
    // duplicate path would make one of them unreachable from the console.
    static_assert(Field::allWithin(device_fields, sizeof(DeviceConfig)), "a device field points outside DeviceConfig");
    static_assert(Field::allWithin(user_fields, sizeof(UserConfig)), "a user field points outside UserConfig");
    static_assert(Field::pathsUnique(device_fields), "duplicate device field path");
    static_assert(Field::pathsUnique(user_fields), "duplicate user field path");
};

inline Registry::Registry(DeviceConfig &device, UserConfig &user) noexcept :
    _sections{
        Section{
            .name = "device",
            .bytes = device.bytes(),
            .fields = {device_fields},
        },
        Section{
            .name = "user",
            .bytes = user.bytes(),
            .fields = {user_fields},
        },
    } {}

}// namespace botix::config
