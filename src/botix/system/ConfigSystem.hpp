// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/Bytes.hpp>
#include <kf/Timer.hpp>
#include <kf/core.hpp>
#include <kf/esp/NVS.hpp>
#include <kf/units.hpp>

#include "botix/cli/Argument.hpp"
#include "botix/cli/Group.hpp"
#include "botix/config/Config.hpp"
#include "botix/config/DeviceConfig.hpp"
#include "botix/config/Registry.hpp"
#include "botix/config/UserConfig.hpp"
#include "botix/protocol/Kind.hpp"
#include "botix/service/ConfigService.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct ConfigSystem : System<ConfigSystem> {

    template<kf::implements<config::ConfigTag> ConfigImpl> struct Strategy {

        static void load(kf::Bytes bytes) noexcept {
            auto maybe_config = ConfigImpl::fromBytes(bytes);
            if (maybe_config.isNone()) { return; }

            auto &config = maybe_config.unwrap();
            if (config.version != ConfigImpl::latest_version) {
                config.reset();
            }
        }

        static void reset(kf::Bytes bytes) noexcept {
            if (auto maybe_config = ConfigImpl::fromBytes(bytes); maybe_config.isSome()) {
                maybe_config.unwrap().reset();
            }
        }

        static void init(service::ConfigService &service) noexcept {
            service.onLoad(load);
            service.resettingStrategy(reset);
            service.requestLoad();
            service.sync();
        }
    };

    constexpr ConfigSystem() noexcept :
        System<ConfigSystem>{{.name{"config"}}} {};

private:
    static constexpr kf::Timer::Config _sync_timer_config{.value = 10'000};

    kf::esp::NVS _nvs{"botix"};

public:
    config::DeviceConfig device{};
    config::UserConfig user{};

    service::ConfigService device_service{{
        .nvs = _nvs,
        .key = "dev-cfg",
        .sync_timer_config = _sync_timer_config,
        .config_bytes = device.bytes(),
    }};

    service::ConfigService user_service{{
        .nvs = _nvs,
        .key = "usr-cfg",
        .sync_timer_config = _sync_timer_config,
        .config_bytes = user.bytes(),
    }};

private:
    config::Registry::ValueField _registry_value_fields[19]{
        // periphery: motor actuator driver (4)
        {"motor.pwm_hz", device.periphery.motor_driver.pwm.frequency_hz},
        {"motor.pwm_bits", device.periphery.motor_driver.pwm.resolution_bits},
        {"motor.max_input", device.periphery.motor_driver.max_input},
        {"motor.dead_zone", device.periphery.motor_driver.duty_dead_zone},

        // periphery: servo actuator driver (6)
        {"servo.pwm_hz", device.periphery.servo.pwm.frequency_hz},
        {"servo.pwm_bits", device.periphery.servo.pwm.resolution_bits},
        {"servo.angle_min", device.periphery.servo.angle_range.start},
        {"servo.angle_max", device.periphery.servo.angle_range.end},
        {"servo.pulse_min", device.periphery.servo.pulse_range.start},
        {"servo.pulse_max", device.periphery.servo.pulse_range.end},

        // periphery: wheel encoder sensor driver (1)
        {"wheel_encoder.mm_per_tick", device.periphery.wheel_odometry_encoder.units_per_tick},

        // components: cli (1)
        {"cli.help_command_description_position", device.cli.help_command_description_position},

        // components: outgoing telemetry (3)
        {"telemetry.wheel_distance.enabled", device.outgoing_telemetry.wheel_distance.enabled},
        {"telemetry.wheel_distance.period_ms", device.outgoing_telemetry.wheel_distance.timer.value},
        {"telemetry.wheel_distance.ahead_ms", device.outgoing_telemetry.wheel_distance.update_ahead_ms},

        // protocol: mavlink (1)
        {"protocol.mavlink.heartbeat_period_ms", device.protocol_registry.mavlink.heartbeat_timer.value},

        // services: mixer (3)
        {"mixer.max_age_ms", device.mixer_service.max_control_input_age_ms},
        {"mixer.left_sign", device.mixer_service.motor_left_sign},
        {"mixer.right_sign", device.mixer_service.motor_right_sign},
    };

    config::Registry::EnumField::Entry const _registry_mixer_service_mode[2]{
        {"direct", service::MixerService::Config::Mode::Direct},
        {"tank", service::MixerService::Config::Mode::Tank},
    };

    config::Registry::EnumField::Entry const _registry_transport_entries[2]{
        {"espnow", transport::Kind::Espnow},
        {"wifi", transport::Kind::Wifi},
    };

    config::Registry::EnumField::Entry const _registry_protocol_entries[2]{
        {"raw", protocol::Kind::Raw},
        {"mavlink", protocol::Kind::Mavlink},
    };

    config::Registry::EnumField _registry_enum_fields[3]{
        {"mixer.mode", device.mixer_service.mode, _registry_mixer_service_mode},
        {"transport.default", user.init_transport_kind, _registry_transport_entries},
        {"protocol.default", user.init_protocol_kind, _registry_protocol_entries},
    };

    config::Registry _registry{
        .value_fields{_registry_value_fields},
        .enum_fields{_registry_enum_fields},
    };

    // TODO: replace with enum
    cli::Argument::String::Item const sync_command_service_argument_options[3]{
        {{.name{"all"}, .shortcut{kf::none}}},
        {{.name{"device"}}},
        {{.name{"user"}}},
    };

    cli::Argument sync_command_arguments[1]{
        {
            {.name{"service"}},
            cli::Argument::String{
                .params{.default_value{"all"}},
                .options{sync_command_service_argument_options},
            },
        },
    };

    cli::Argument field_command_arguments[2]{
        {
            {.name{"path"}},
            cli::Argument::String{},
        },
        {
            {.name{"value"}},
            cli::Argument::String{
                .params{.default_value{""}},
            },
        },
    };

    BOTIX_IMPL_SYSTEM(ConfigSystem);

    void onSetupImpl() noexcept {
        if (_nvs.init().isError()) {
            this->_logger.error("NVS init failed");
        }

        Strategy<config::DeviceConfig>::init(device_service);
        Strategy<config::UserConfig>::init(user_service);
    }

    void setupCliImpl(kf::Arena &arena, cli::Group &group) noexcept {

        (void) group.addCommand(arena, {.name{"sync"}}, sync_command_arguments, [this](cli::Command::Context const &context) -> void {
            auto const target = context.arguments[0].string();

            if (target == "all" or target == "device") {
                device_service.sync();
            }

            if (target == "all" or target == "user") {
                user_service.sync();
            }

            context.channel.output.print("sync requested for {} service(s)", target);
        });

        (void) group.addCommand(arena, {.name{"list"}}, {}, [this](cli::Command::Context const &context) -> void {
            context.channel.output.print("Available fields:");

            for (auto const &entry: _registry.value_fields) {
                context.channel.output.print("{}", entry);
            }

            for (auto const &entry: _registry.enum_fields) {
                context.channel.output.print("{}", entry);
            }
        });

        (void) group.addCommand(arena, {.name{"field"}}, field_command_arguments, [this](cli::Command::Context const &context) -> void {
            auto const path = context.arguments[0].string();
            auto const lexeme = context.arguments[1].string();

            if (not lexeme.empty()) {
                // set

                auto const result = _registry.set(path, lexeme);
                if (result.isOk()) {
                    context.channel.output.print("field updated");
                } else {
                    context.channel.output.error("set failed: {}", result.error().message);
                }
            }

            // get

            if (auto field = _registry.findValueField(path); field.isSome()) {
                context.channel.output.print("{}", field.unwrap());
                return;
            }

            if (auto field = _registry.findEnumField(path); field.isSome()) {
                context.channel.output.print("{}", field.unwrap());
                return;
            }

            context.channel.output.error("field '{}' not found", path);
        });

        // TODO: reset command
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        device_service.poll(now);
        user_service.poll(now);
    }
};

}// namespace botix::system