// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/Slice.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include "botix/config/Access.hpp"
#include "botix/config/DeviceConfig.hpp"
#include "botix/config/Field.hpp"
#include "botix/config/UserConfig.hpp"

/// @brief Byte offset of a possibly nested member
/// @note The config structs carry empty CRTP bases, so they are not standard layout
///       and `offsetof` is only conditionally supported. GCC computes it correctly
///       for these aggregates; the diagnostic is suppressed around the tables below.
#define BOTIX_CONFIG_OFFSET(__type__, ...) \
    static_cast<kf::u16>(offsetof(__type__, __VA_ARGS__))

namespace botix::config {

namespace internal {

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

inline constexpr EnumOption encoder_pull_options[]{
    {"external", static_cast<kf::usize>(kf::gpio::DigitalInput::Pull::External)},
    {"pulldown", static_cast<kf::usize>(kf::gpio::DigitalInput::Pull::InternalDown)},
    {"pullup", static_cast<kf::usize>(kf::gpio::DigitalInput::Pull::InternalUp)},
};

inline constexpr EnumOption mixer_mode_options[]{
    {"direct", static_cast<kf::usize>(botix::internal::MixerServiceConfig::Mode::Direct)},
    {"tank", static_cast<kf::usize>(botix::internal::MixerServiceConfig::Mode::Tank)},
};

inline constexpr EnumOption transport_kind_options[]{
    {"espnow", static_cast<kf::usize>(transport::Kind::Espnow)},
    {"wifi", static_cast<kf::usize>(transport::Kind::Wifi)},
};

inline constexpr EnumOption protocol_kind_options[]{
    {"raw", static_cast<kf::usize>(protocol::Kind::Raw)},
    {"mavlink", static_cast<kf::usize>(protocol::Kind::Mavlink)},
};

/// @brief Every addressable field of `DeviceConfig`
/// @note `version` is deliberately absent: it is owned by the migration logic
inline constexpr Field device_fields[]{
    // periphery / motors
    BOTIX_FIELD(DeviceConfig, "motor.max_input", periphery.motor_driver.max_input, FieldKind::Signed, kf::i16),
    BOTIX_FIELD(DeviceConfig, "motor.dead_zone", periphery.motor_driver.duty_dead_zone, FieldKind::Unsigned, kf::u16),
    BOTIX_FIELD(DeviceConfig, "motor.pwm_hz", periphery.motor_driver.pwm.frequency_hz, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "motor.pwm_bits", periphery.motor_driver.pwm.resolution_bits, FieldKind::Unsigned, kf::u8),

    // periphery / servos
    BOTIX_FIELD(DeviceConfig, "servo.angle_min", periphery.servo.angle_range.start, FieldKind::Unsigned, kf::u16),
    BOTIX_FIELD(DeviceConfig, "servo.angle_max", periphery.servo.angle_range.end, FieldKind::Unsigned, kf::u16),
    BOTIX_FIELD(DeviceConfig, "servo.pulse_min", periphery.servo.pulse_range.start, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "servo.pulse_max", periphery.servo.pulse_range.end, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "servo.pwm_hz", periphery.servo.pwm.frequency_hz, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "servo.pwm_bits", periphery.servo.pwm.resolution_bits, FieldKind::Unsigned, kf::u8),

    // periphery / encoders
    BOTIX_FIELD(DeviceConfig, "encoder.mm_per_tick", periphery.wheel_odometry_encoder.units_per_tick, FieldKind::Real, kf::f64),
    Field{
        .path = "encoder.pull",
        .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, periphery.wheel_odometry_encoder.pull),
        .size = sizeof(kf::gpio::DigitalInput::Pull),
        .kind = FieldKind::Enumerated,
        .secret = false,
        .options = {encoder_pull_options},
        .min = kf::none,
        .max = kf::none,
    },

    // telemetry
    BOTIX_FIELD(DeviceConfig, "telemetry.wheel.period_ms", outgoing_telemetry.wheel_distance.timer.value, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "telemetry.wheel.ahead_ms", outgoing_telemetry.wheel_distance.update_ahead_ms, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "telemetry.wheel.enabled", outgoing_telemetry.wheel_distance.enabled, FieldKind::Boolean, bool),

    // mavlink protocol
    BOTIX_FIELD(DeviceConfig, "mavlink.heartbeat_ms", protocol_registry.mavlink.heartbeat_timer.value, FieldKind::Unsigned, kf::u32),
    BOTIX_FIELD(DeviceConfig, "mavlink.sysid_self", protocol_registry.mavlink.system_id_self, FieldKind::Unsigned, kf::u8),
    BOTIX_FIELD(DeviceConfig, "mavlink.sysid_target", protocol_registry.mavlink.system_id_target, FieldKind::Unsigned, kf::u8),
    BOTIX_FIELD(DeviceConfig, "mavlink.compid_heartbeat", protocol_registry.mavlink.component_id_heartbeat, FieldKind::Unsigned, kf::u8),
    BOTIX_FIELD(DeviceConfig, "mavlink.compid_wheel", protocol_registry.mavlink.component_id_wheel_distance, FieldKind::Unsigned, kf::u8),

    // mixer
    BOTIX_FIELD(DeviceConfig, "mixer.max_input_age_ms", mixer_service.max_control_input_age_ms, FieldKind::Unsigned, kf::usize),
    Field{
        .path = "mixer.mode",
        .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.mode),
        .size = sizeof(botix::internal::MixerServiceConfig::Mode),
        .kind = FieldKind::Enumerated,
        .secret = false,
        .options = {mixer_mode_options},
        .min = kf::none,
        .max = kf::none,
    },
    Field{
        .path = "mixer.left_sign",
        .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.motor_left_sign),
        .size = sizeof(kf::i8),
        .kind = FieldKind::Signed,
        .secret = false,
        .options = {},
        .min = kf::some(-1.0),
        .max = kf::some(1.0),
    },
    Field{
        .path = "mixer.right_sign",
        .offset = BOTIX_CONFIG_OFFSET(DeviceConfig, mixer_service.motor_right_sign),
        .size = sizeof(kf::i8),
        .kind = FieldKind::Signed,
        .secret = false,
        .options = {},
        .min = kf::some(-1.0),
        .max = kf::some(1.0),
    },
};

/// @brief Every addressable field of `UserConfig`
inline constexpr Field user_fields[]{
    Field{
        .path = "boot.transport",
        .offset = BOTIX_CONFIG_OFFSET(UserConfig, init_transport_kind),
        .size = sizeof(transport::Kind),
        .kind = FieldKind::Enumerated,
        .secret = false,
        .options = {transport_kind_options},
        .min = kf::none,
        .max = kf::none,
    },
    Field{
        .path = "boot.protocol",
        .offset = BOTIX_CONFIG_OFFSET(UserConfig, init_protocol_kind),
        .size = sizeof(protocol::Kind),
        .kind = FieldKind::Enumerated,
        .secret = false,
        .options = {protocol_kind_options},
        .min = kf::none,
        .max = kf::none,
    },

    // wifi station
    BOTIX_FIELD(UserConfig, "wifi.enabled", network.enabled, FieldKind::Boolean, bool),
    BOTIX_FIELD(UserConfig, "wifi.ssid", network.ssid, FieldKind::Text, decltype(UserConfig::network.ssid)),
    Field{
        .path = "wifi.password",
        .offset = BOTIX_CONFIG_OFFSET(UserConfig, network.password),
        .size = static_cast<kf::u16>(sizeof(decltype(UserConfig::network.password))),
        .kind = FieldKind::Text,
        .secret = true,
        .options = {},
        .min = kf::none,
        .max = kf::none,
    },
    BOTIX_FIELD(UserConfig, "wifi.hostname", network.hostname, FieldKind::Text, decltype(UserConfig::network.hostname)),
    BOTIX_FIELD(UserConfig, "wifi.retry_ms", network.retry_timer.value, FieldKind::Unsigned, kf::u32),

    // udp transport
    BOTIX_FIELD(UserConfig, "udp.local_port", transport_registry.wifi_udp.local_port, FieldKind::Unsigned, kf::u16),
    BOTIX_FIELD(UserConfig, "udp.remote_ip", transport_registry.wifi_udp.remote.address, FieldKind::Ipv4, kf::u32),
    BOTIX_FIELD(UserConfig, "udp.remote_port", transport_registry.wifi_udp.remote.port, FieldKind::Unsigned, kf::u16),
};

#pragma GCC diagnostic pop

#undef BOTIX_FIELD

/// @brief Every field must address bytes that actually belong to its config struct
template<kf::usize N> [[nodiscard]] constexpr bool fieldsWithin(Field const (&fields)[N], kf::usize config_size) noexcept {
    for (auto const &field: fields) {
        if (kf::usize{field.offset} + kf::usize{field.size} > config_size) {
            return false;
        }
    }
    return true;
}

/// @brief Paths are the console's addressing scheme, so they must be unique per section
template<kf::usize N> [[nodiscard]] constexpr bool pathsUnique(Field const (&fields)[N]) noexcept {
    for (kf::usize i = 0; i < N; i += 1) {
        for (kf::usize j = i + 1; j < N; j += 1) {
            if (fields[i].path == fields[j].path) {
                return false;
            }
        }
    }
    return true;
}

static_assert(fieldsWithin(device_fields, sizeof(DeviceConfig)), "a device field points outside DeviceConfig");
static_assert(fieldsWithin(user_fields, sizeof(UserConfig)), "a user field points outside UserConfig");
static_assert(pathsUnique(device_fields), "duplicate device field path");
static_assert(pathsUnique(user_fields), "duplicate user field path");

}// namespace internal

/// @brief Name-addressable view over every persisted configuration field
struct Registry {

    /// @brief Number of sections; kept as a constant so the array needs no allocation
    static constexpr kf::usize section_count{2};

    explicit Registry(DeviceConfig &device, UserConfig &user) noexcept :
        _sections{
            Section{
                .name = "device",
                .bytes = device.bytes(),
                .fields = {internal::device_fields},
            },
            Section{
                .name = "user",
                .bytes = user.bytes(),
                .fields = {internal::user_fields},
            },
        } {}

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
};

}// namespace botix::config
