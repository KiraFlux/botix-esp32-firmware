// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include "botix/config/Access.hpp"
#include "botix/config/Registry.hpp"
#include "botix/service/ConfigService.hpp"
#include "botix/service/ConsoleService.hpp"

namespace botix::command {

namespace internal {

using Output = service::ConsoleService::Channel::Output;

/// @brief Sub-command selected by the first argument of `config`
enum class ConfigAction : kf::u8 {
    List,
    Get,
    Set,
    Save,
    ResetDevice,
    ResetUser,
};

/// @brief Render a field's current value, masking anything marked secret
inline void printValue(Output &output, config::Section const &section, config::Field const &field) noexcept {
    if (field.secret) {
        // The console is unauthenticated; never echo a stored secret back
        auto const stored = config::access::readText(section, field);
        output.print("{}.{} = {}", section.name, field.path, stored.empty() ? "(unset)" : "(set)");
        return;
    }

    switch (field.kind) {
        case config::FieldKind::Boolean:
            output.print("{}.{} = {}", section.name, field.path, config::access::readBoolean(section, field));
            return;

        case config::FieldKind::Signed:
            output.print("{}.{} = {}", section.name, field.path, config::access::readSigned(section, field));
            return;

        case config::FieldKind::Unsigned:
            output.print("{}.{} = {}", section.name, field.path, config::access::readUnsigned(section, field));
            return;

        case config::FieldKind::Real:
            output.print("{}.{} = {}", section.name, field.path, config::access::readReal(section, field));
            return;

        case config::FieldKind::Enumerated:
            output.print("{}.{} = {}", section.name, field.path, config::access::readOptionName(section, field));
            return;

        case config::FieldKind::Text:
            output.print("{}.{} = '{}'", section.name, field.path, config::access::readText(section, field));
            return;

        case config::FieldKind::Ipv4: {
            auto const address = config::access::readIpv4(section, field);
            output.print(
                "{}.{} = {}.{}.{}.{}",
                section.name, field.path,
                (address >> 24) & 0xff,
                (address >> 16) & 0xff,
                (address >> 8) & 0xff,
                address & 0xff);
            return;
        }

        default:
            output.print("{}.{} = ?", section.name, field.path);
            return;
    }
}

/// @brief Print every field whose section name or path starts with `prefix`
inline void listFields(Output &output, config::Registry const &registry, kf::StringView prefix) noexcept {
    for (auto const &section: registry.sections()) {
        for (auto const &field: section.fields) {
            if (not prefix.empty() and not section.name.startsWith(prefix) and not field.path.startsWith(prefix)) {
                continue;
            }

            printValue(output, section, field);
        }
    }
}

}// namespace internal

/// @brief Register the `config` command
/// @return false when the console ran out of command or argument capacity
[[nodiscard]] inline bool registerConfigCommands(
    service::ConsoleService &console,
    kf::Arena &arena,
    config::Registry &registry,
    service::ConfigService &device_service,
    service::ConfigService &user_service) noexcept {

    struct Context {
        config::Registry &registry;
        service::ConfigService &device_service;
        service::ConfigService &user_service;
    };

    // Captured by reference; all three outlive the console
    static Context context{registry, device_service, user_service};

    auto maybe_command = console.addCommand(arena, "config", [](auto const &call) {
        auto const action = static_cast<internal::ConfigAction>(call.arguments[0].enumIndex());
        auto const path = call.arguments[1].string();
        auto const value = call.arguments[2].string();

        switch (action) {
            case internal::ConfigAction::List: {
                internal::listFields(call.output, context.registry, path);
                return;
            }

            case internal::ConfigAction::Save: {
                context.device_service.sync();
                context.user_service.sync();
                call.output.print("configuration written to NVS");
                return;
            }

            case internal::ConfigAction::ResetDevice: {
                context.device_service.requestReset();
                context.device_service.sync();
                call.output.print("device config reset; reboot to re-init hardware from defaults");
                return;
            }

            case internal::ConfigAction::ResetUser: {
                context.user_service.requestReset();
                context.user_service.sync();
                call.output.print("user config reset; reboot to re-init from defaults");
                return;
            }

            case internal::ConfigAction::Get: {
                if (path.empty()) {
                    call.output.error("get needs a field path, for example 'user.wifi.ssid'");
                    return;
                }

                auto const resolved = context.registry.resolve(path);
                if (resolved.isNone()) {
                    call.output.error("unknown field '{}'", path);
                    return;
                }

                internal::printValue(call.output, resolved.unwrap().section, resolved.unwrap().field);
                return;
            }

            case internal::ConfigAction::Set: {
                if (path.empty() or value.empty()) {
                    call.output.error("set needs a field path and a value");
                    return;
                }

                auto const resolved = context.registry.resolve(path);
                if (resolved.isNone()) {
                    call.output.error("unknown field '{}'", path);
                    return;
                }

                auto const &section = resolved.unwrap().section;
                auto const &field = resolved.unwrap().field;

                auto const status = config::access::set(section, field, value);

                if (status != config::access::SetStatus::Ok) {
                    call.output.error("{}: {}", path, config::access::statusName(status));

                    if (field.kind == config::FieldKind::Enumerated) {
                        for (auto const &option: field.options) {
                            call.output.print("  '{}'", option.label());
                        }
                    }
                    return;
                }

                internal::printValue(call.output, section, field);
                call.output.print("note: 'config save' persists, some fields apply on reboot");
                return;
            }

            default:
                call.output.error("unhandled action");
                return;
        }
    });

    if (maybe_command.isNone()) {
        return false;
    }

    auto &command = maybe_command.unwrap();

    static service::ConsoleService::Command::Argument::EnumItem const actions[]{
        {"list", internal::ConfigAction::List},
        {"get", internal::ConfigAction::Get},
        {"set", internal::ConfigAction::Set},
        {"save", internal::ConfigAction::Save},
        {"reset-device", internal::ConfigAction::ResetDevice},
        {"reset-user", internal::ConfigAction::ResetUser},
    };

    if (not command.addEnumArgument("action", {.items{actions}})) { return false; }

    // Both are optional: only `get` and `set` consume them
    if (not command.addStringArgument("path", {.params{.default_value = kf::some(kf::StringView{""})}})) { return false; }
    if (not command.addStringArgument("value", {.params{.default_value = kf::some(kf::StringView{""})}})) { return false; }

    return true;
}

}// namespace botix::command
