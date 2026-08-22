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
    config::Registry::EnumItem const _registry_mixer_service_mode[2]{
        {"direct", service::MixerService::Config::Mode::Direct},
        {"tank", service::MixerService::Config::Mode::Tank},
    };

    config::Registry::EnumItem const _registry_transport_entries[2]{
        {"espnow", transport::Kind::Espnow},
        {"wifi", transport::Kind::Wifi},
    };

    config::Registry::EnumItem const _registry_protocol_entries[2]{
        {"raw", protocol::Kind::Raw},
        {"mavlink", protocol::Kind::Mavlink},
    };

    config::Registry::Field _registry_fields[22]{
        // user: (2)
        {"user.boot.transport", user.boot_transport_kind, _registry_transport_entries},
        {"user.boot.protocol", user.boot_protocol_kind, _registry_protocol_entries},

        // services: mixer (3)
        {"mixer.mode", device.mixer_service.mode, _registry_mixer_service_mode},
        {"mixer.max_age_ms", device.mixer_service.max_control_input_age_ms},
        {"mixer.left_sign", device.mixer_service.motor_left_sign},
        {"mixer.right_sign", device.mixer_service.motor_right_sign},

        // telemetry: (3)
        {"telemetry.wheel_distance.enabled", device.outgoing_telemetry.wheel_distance.enabled},
        {"telemetry.wheel_distance.period_ms", device.outgoing_telemetry.wheel_distance.timer.value},
        {"telemetry.wheel_distance.ahead_ms", device.outgoing_telemetry.wheel_distance.update_ahead_ms},

        // protocol: (1)
        {"protocol.mavlink.heartbeat_period_ms", device.protocol_registry.mavlink.heartbeat_timer.value},

        // periphery: wheel_encoder (1)
        {"wheel_encoder.mm_per_tick", device.periphery.wheel_odometry_encoder.units_per_tick},

        // periphery: motor (4)
        {"motor.pwm_hz", device.periphery.motor_driver.pwm.frequency_hz},
        {"motor.pwm_bits", device.periphery.motor_driver.pwm.resolution_bits},
        {"motor.max_input", device.periphery.motor_driver.max_input},
        {"motor.dead_zone", device.periphery.motor_driver.duty_dead_zone},

        // periphery: servo (6)
        {"servo.pwm_hz", device.periphery.servo.pwm.frequency_hz},
        {"servo.pwm_bits", device.periphery.servo.pwm.resolution_bits},
        {"servo.angle_min", device.periphery.servo.angle_range.start},
        {"servo.angle_max", device.periphery.servo.angle_range.end},
        {"servo.pulse_min", device.periphery.servo.pulse_range.start},
        {"servo.pulse_max", device.periphery.servo.pulse_range.end},

        // cli (1)
        {"cli.help_command_description_position", device.cli.help_command_description_position},
    };

    config::Registry _registry{_registry_fields};

    enum class ServiceKind {
        Device,
        User,
    };

    cli::Argument::Enum::Item const config_service_kinds[2]{
        {{.name{"device"}}, ServiceKind::Device},
        {{.name{"user"}}, ServiceKind::User},
    };

    cli::Argument service_related_command_arguments[1]{
        {
            {.name{"target_service"}},
            cli::Argument::Enum{
                .items{config_service_kinds},
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

    [[nodiscard]] constexpr service::ConfigService &getService(ServiceKind kind) noexcept {
        switch (kind) {
            case ServiceKind::User:
                return user_service;

            case ServiceKind::Device:
            default:
                return device_service;
        }
    }

    BOTIX_IMPL_SYSTEM(ConfigSystem);

    void onSetupImpl() noexcept {
        if (_nvs.init().isError()) {
            this->_logger.error("NVS init failed");
        }

        Strategy<config::DeviceConfig>::init(device_service);
        Strategy<config::UserConfig>::init(user_service);
    }

    void setupCliImpl(kf::Arena &arena, cli::Group &group) noexcept {
        (void) group.addCommand(arena, {.name{"list"}}, {}, [this](cli::Command::Context const &context) -> void {
            context.channel.output.print("Available fields:");

            for (auto const &entry: _registry.all()) {
                context.channel.output.print("{}", entry);
            }
        });

        (void) group.addCommand(arena, {.name{"sync"}}, service_related_command_arguments, [this](cli::Command::Context const &context) -> void {
            auto const target_kind = context.arguments[0].enumValue<ServiceKind>();
            auto const target_name = context.arguments[0].enumName();

            getService(target_kind).sync();
            context.channel.output.print("sync completed for '{}' service", target_name);
        });

        (void) group.addCommand(arena, {.name{"reset"}}, service_related_command_arguments, [this](cli::Command::Context const &context) -> void {
            auto const target_kind = context.arguments[0].enumValue<ServiceKind>();
            auto const target_name = context.arguments[0].enumName();

            context.channel.output.print("reset requested for '{}' service", target_name);
            getService(target_kind).requestReset();
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

            if (auto field = _registry.get(path); field.isSome()) {
                context.channel.output.print("{}", field.unwrap());
                return;
            }

            context.channel.output.error("field '{}' not found", path);
        });
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        device_service.poll(now);
        user_service.poll(now);
    }
};

}// namespace botix::system