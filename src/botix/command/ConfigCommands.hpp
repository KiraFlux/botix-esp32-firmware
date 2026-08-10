// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/config/Access.hpp"
#include "botix/config/Field.hpp"
#include "botix/config/Kind.hpp"
#include "botix/config/Registry.hpp"
#include "botix/config/Section.hpp"
#include "botix/config/SetStatus.hpp"
#include "botix/service/ConfigService.hpp"
#include "botix/service/ConsoleService.hpp"

namespace botix::command {

/// @brief The `config` command: inspect and change persisted settings by name
struct ConfigCommands : kf::mixin::NonCopyable {

    struct Dependencies {
        config::Registry &registry;
        service::ConfigService &device_service;
        service::ConfigService &user_service;
    };

    explicit constexpr ConfigCommands(Dependencies deps) noexcept :
        _registry{deps.registry},
        _device_service{deps.device_service},
        _user_service{deps.user_service} {}

    [[nodiscard]] bool registerIn(service::ConsoleService &console, kf::Arena &arena) noexcept {
        auto maybe_command = console.addCommand(arena, "config", [this](auto const &call) { execute(call); });

        if (maybe_command.isNone()) {
            return false;
        }

        auto &command = maybe_command.unwrap();

        if (not command.addEnumArgument("action", {.items{_actions}})) { return false; }

        // Both are optional: only `get` and `set` consume them
        if (not command.addStringArgument("path", {.params{.default_value = kf::some(kf::StringView{""})}})) { return false; }
        if (not command.addStringArgument("value", {.params{.default_value = kf::some(kf::StringView{""})}})) { return false; }

        return true;
    }

private:
    using Output = service::ConsoleService::Channel::Output;
    using Call = service::ConsoleService::Command::Context;
    using EnumItem = service::ConsoleService::Command::Argument::EnumItem;

    enum class Action : kf::u8 {
        List,
        Get,
        Set,
        Save,
        ResetDevice,
        ResetUser,
    };

    static constexpr EnumItem _actions[]{
        {"list", Action::List},
        {"get", Action::Get},
        {"set", Action::Set},
        {"save", Action::Save},
        {"reset-device", Action::ResetDevice},
        {"reset-user", Action::ResetUser},
    };

    config::Registry &_registry;
    service::ConfigService &_device_service;
    service::ConfigService &_user_service;

    void execute(Call const &call) noexcept {
        auto const action = static_cast<Action>(call.arguments[0].enumIndex());
        auto const path = call.arguments[1].string();
        auto const value = call.arguments[2].string();

        switch (action) {
            case Action::List: return list(call.output, path);
            case Action::Get: return get(call.output, path);
            case Action::Set: return set(call.output, path, value);
            case Action::Save: return save(call.output);
            case Action::ResetDevice: return reset(call.output, _device_service, "device");
            case Action::ResetUser: return reset(call.output, _user_service, "user");
            default: return call.output.error("unhandled action");
        }
    }

    /// @brief Print every field whose section name or path starts with `prefix`
    void list(Output &output, kf::StringView prefix) const noexcept {
        for (auto const &section: _registry.sections()) {
            for (auto const &field: section.fields) {
                if (not prefix.empty() and not section.name.startsWith(prefix) and not field.path.startsWith(prefix)) {
                    continue;
                }

                printValue(output, section, field);
            }
        }
    }

    void get(Output &output, kf::StringView path) const noexcept {
        if (path.empty()) {
            output.error("get needs a field path, for example 'user.wifi.ssid'");
            return;
        }

        auto const resolved = _registry.resolve(path);
        if (resolved.isNone()) {
            output.error("unknown field '{}'", path);
            return;
        }

        printValue(output, resolved.unwrap().section, resolved.unwrap().field);
    }

    void set(Output &output, kf::StringView path, kf::StringView value) const noexcept {
        if (path.empty() or value.empty()) {
            output.error("set needs a field path and a value");
            return;
        }

        auto const resolved = _registry.resolve(path);
        if (resolved.isNone()) {
            output.error("unknown field '{}'", path);
            return;
        }

        auto const &section = resolved.unwrap().section;
        auto const &field = resolved.unwrap().field;

        auto const status = config::Access::set(section, field, value);

        if (status != config::SetStatus::Ok) {
            output.error("{}: {}", path, config::name(status));

            if (field.kind == config::Kind::Enumerated) {
                for (auto const &option: field.options) {
                    output.print("  '{}'", option.label());
                }
            }
            return;
        }

        printValue(output, section, field);
        output.print("note: 'config save' persists, some fields apply on reboot");
    }

    void save(Output &output) noexcept {
        _device_service.sync();
        _user_service.sync();
        output.print("configuration written to NVS");
    }

    static void reset(Output &output, service::ConfigService &service, kf::StringView which) noexcept {
        service.requestReset();
        service.sync();
        output.print("{} config reset; reboot to re-init from defaults", which);
    }

    /// @brief Render a field's current value, masking anything marked secret
    static void printValue(Output &output, config::Section const &section, config::Field const &field) noexcept {
        if (field.secret) {
            // The console is unauthenticated; never echo a stored secret back
            auto const stored = config::Access::readText(section, field);
            output.print("{}.{} = {}", section.name, field.path, stored.empty() ? "(unset)" : "(set)");
            return;
        }

        switch (field.kind) {
            case config::Kind::Boolean:
                return output.print("{}.{} = {}", section.name, field.path, config::Access::readBoolean(section, field));

            case config::Kind::Signed:
                return output.print("{}.{} = {}", section.name, field.path, config::Access::readSigned(section, field));

            case config::Kind::Unsigned:
                return output.print("{}.{} = {}", section.name, field.path, config::Access::readUnsigned(section, field));

            case config::Kind::Real:
                return output.print("{}.{} = {}", section.name, field.path, config::Access::readReal(section, field));

            case config::Kind::Enumerated:
                return output.print("{}.{} = {}", section.name, field.path, config::Access::readOptionName(section, field));

            case config::Kind::Text:
                return output.print("{}.{} = '{}'", section.name, field.path, config::Access::readText(section, field));

            case config::Kind::Ipv4:
                return output.print("{}.{} = {}", section.name, field.path, config::Access::readIpv4(section, field));

            default:
                return output.print("{}.{} = ?", section.name, field.path);
        }
    }
};

}// namespace botix::command
