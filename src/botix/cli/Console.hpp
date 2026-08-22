// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/Logger.hpp>
#include <kf/Option.hpp>
#include <kf/Registry.hpp>
#include <kf/StringView.hpp>
#include <kf/core.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/ExtraAllocationLength.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Poll.hpp>

#include "botix/cli/Channel.hpp"
#include "botix/cli/Config.hpp"
#include "botix/cli/Group.hpp"
#include "botix/cli/Identifier.hpp"

namespace botix::internal {

using ChannelRegistryBase = kf::Registry<cli::Channel>;

template<typename Impl> struct ChannelRegistry : private ChannelRegistryBase {

    using ChannelRegistryBase::ChannelRegistryBase;

    [[nodiscard]] decltype(auto) channels() noexcept {
        return this->items();
    }

    [[nodiscard]] decltype(auto) channels() const noexcept {
        return this->items();
    }

    [[nodiscard]] decltype(auto) addChannel(kf::Arena &arena, cli::Channel::Parameters params) noexcept {
        return this->add(arena, static_cast<Impl const *>(this)->config(), params);
    }
};

using ConsoleGroupRegistryBase = kf::Registry<cli::Group>;

template<typename Impl> struct ConsoleGroupRegistry : private ConsoleGroupRegistryBase {

    explicit ConsoleGroupRegistry(kf::Arena &arena, kf::usize max_group_count) noexcept :
        ConsoleGroupRegistryBase{arena, max_group_count} {
        // caller ensure
        (void) this->addGroup(
            arena,
            {
                .name{"global"},
                .description{"Common commands. Prefix is optional."},
                .shortcut{kf::none},
            });

        auto should_be_command = this->globalGroup().addCommand(
            arena,
            {
                .name{"help"},
                .description{"Show help about command or group."},
            },
            help_command_arguments,
            [this](cli::Command::Context const &context) -> void {
                auto &output = context.channel.output;
                auto const target = context.arguments[0].string();

                if (auto const &maybe_command = this->resolveCommand(target); maybe_command.isSome()) {
                    writeCommandHelp(output.string, maybe_command.unwrap(), false);
                    return;
                }

                if (auto const &maybe_group = this->getGroup(target); maybe_group.isSome()) {
                    writeGroupHelp(output.string, maybe_group.unwrap());
                    return;
                }

                if (not target.empty()) {
                    output.error("'{}' is not a valid group or command.", target);
                }

                for (auto const ns: this->groups()) {
                    writeGroupHelp(output.string, *ns);
                }
            });
    }

    [[nodiscard]] decltype(auto) groups() noexcept {
        return this->items();
    }

    [[nodiscard]] decltype(auto) groups() const noexcept {
        return this->items();
    }

    [[nodiscard]] decltype(auto) globalGroup() noexcept {
        return *groups()[0];
    }

    [[nodiscard]] decltype(auto) globalGroup() const noexcept {
        return *groups()[0];
    }

    [[nodiscard]] decltype(auto) getGroup(kf::StringView name_or_shortcut) noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] decltype(auto) getGroup(kf::StringView name_or_shortcut) const noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] decltype(auto) addGroup(kf::Arena &arena, cli::Identifier id) {
        return this->add(arena, static_cast<Impl const *>(this)->config(), id);
    }

    [[nodiscard]] auto resolveCommand(kf::StringView path) noexcept -> kf::Option<cli::Command &> {
        auto const maybe_delimeter_index = path.indexOf('.');

        if (maybe_delimeter_index.isNone()) {
            return globalGroup().getCommand(path);
        }

        auto const delimeter_index = maybe_delimeter_index.unwrap();

        auto maybe_group = getGroup(path.first(delimeter_index));
        if (maybe_group.isNone()) { return kf::none; }

        return maybe_group.unwrap().getCommand(path.fromOffset(delimeter_index + 1));
    }

protected:
    void writeCommandHelp(auto &char_writable, cli::Command const &command, bool inline_description) const noexcept {
        if (not inline_description) {
            (void) char_writable.append("\nCommand:\n  ");
            (void) char_writable.append(command.name);

            if (command.shortcut.isSome()) {
                (void) char_writable.append('/');
                (void) char_writable.append(command.shortcut.unwrap());
            }

            if (not command.description.empty()) {
                (void) char_writable.append(" - ");
                (void) char_writable.append(command.description);
            }

            (void) char_writable.append("\nUsage:\n  ");
        }

        auto const write_length = char_writable.append(command);

        if (inline_description and not command.description.empty()) {
            for (int i = static_cast<Impl const *>(this)->config().help_command_description_position; i > write_length; i -= 1) {
                (void) char_writable.append(' ');
            }
            (void) char_writable.append(" - ");
            (void) char_writable.append(command.description);
        }
        (void) char_writable.append('\n');
    }

    void writeGroupHelp(auto &char_writable, cli::Group const &group) const noexcept {
        (void) char_writable.append("\nGroup:\n  ");
        (void) char_writable.append(group.name);

        if (group.shortcut.isSome()) {
            (void) char_writable.append('/');
            (void) char_writable.append(group.shortcut.unwrap());
        }

        if (not group.description.empty()) {
            (void) char_writable.append(" - ");
            (void) char_writable.append(group.description);
        }

        (void) char_writable.append("\nCommands:\n");

        for (auto const c: group.commands()) {
            (void) char_writable.appendFormat("  {}.", group.name);
            writeCommandHelp(char_writable, *c, true);
        }
    }

private:
    cli::Argument help_command_arguments[1]{
        {
            {
                .name{"target"},
                .description{"resolvable name"},
            },
            cli::Argument::String{
                .params{.default_value{""}},
            },
        },
    };
};

}// namespace botix::internal

namespace botix::cli {

struct Console final :

    kf::mixin::Configured<Config>,
    internal::ChannelRegistry<Console>,
    internal::ConsoleGroupRegistry<Console>,
    kf::mixin::NonCopyable,
    kf::mixin::Poll<Console>,
    kf::mixin::ExtraAllocationLength<Console>

{

    explicit Console(kf::Arena &arena, Config const &config) noexcept :
        kf::mixin::Configured<Config>{config},
        internal::ChannelRegistry<Console>{arena, config.max_channel_count},
        internal::ConsoleGroupRegistry<Console>{arena, config.max_group_count} {}

private:
    static constexpr kf::usize max_tokens_count{8};

    kf::Logger _logger{"Console"};

    void onInputLineReady(Channel::Context const &channel_context) noexcept {
        if (channel_context.parameters.echo) {
            channel_context.output.print("[#{}]>>> {}", channel_context.num, channel_context.input_line);
        }

        kf::StringView tokens_buffer[max_tokens_count]{};

        auto tokens = channel_context.input_line.trim().split({tokens_buffer});

        if (tokens.empty()) {
            return;
        }

        auto const name = tokens[0];
        auto maybe_command = this->resolveCommand(name);

        if (maybe_command.isNone()) {
            channel_context.output.error("unknown command: {}", name);
            return;
        }

        auto &command = maybe_command.unwrap();
        auto argument_tokens = tokens.fromOffset(1);
        // TODO: move to Command
        if (argument_tokens.length() < command.positionalArgumentsCount() or argument_tokens.length() > command.arguments().length()) {
            channel_context.output.error("expected {}..{} arguments, got {}", command.positionalArgumentsCount(), command.arguments().length(), argument_tokens.length());
            this->writeCommandHelp(channel_context.output.string, command, false);
            return;
        }

        auto argument_index = 0;
        bool parse_failed = false;
        while (argument_index < argument_tokens.length()) {
            auto lexeme = argument_tokens[argument_index];
            auto &argument = command.arguments()[argument_index];

            if (not argument.parse({.channel_output = channel_context.output, .lexeme = lexeme})) {
                parse_failed = true;
                channel_context.output.print("note: failed argument '{}'", argument.name);
            }

            argument_index += 1;
        }

        if (parse_failed) {
            channel_context.output.error("failed to parse positional argument(s)");
            this->writeCommandHelp(channel_context.output.string, command, false);
            return;
        }

        auto default_value_arguments = command.arguments().fromOffset(argument_index);
        for (auto &a: default_value_arguments) {
            a.reset();
        }

        command.execute(channel_context);
    }

    KF_IMPL_POLL(Console);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        for (kf::u8 channel_num = 0; channel_num < this->channels().length(); channel_num += 1) {
            auto &channel = *this->channels()[channel_num];

            auto const status = channel.input.process();
            using S = decltype(status);

            switch (status) {
                case S::Idle:
                    break;

                case S::HintRequested:
                    _logger.debug("hint: '{}'", channel.input.peekLine());// TODO: parse, suggestion depends on input
                    break;

                case S::LineReady:
                    onInputLineReady({
                        .input_line = channel.input.consumeLine(),
                        .parameters = channel.parameters,
                        .output = channel.output,
                        .timestamp = now,
                        .num = channel_num,
                    });
                    break;
            }
        }
    }

    KF_IMPL_EXTRA_ALLOCATION_LENGTH(Console);
    static constexpr auto getExtraAllocationLengthImpl(Config const &config) noexcept {
        return static_cast<kf::usize>(config.max_channel_count * sizeof(Channel) + config.max_group_count * sizeof(Group));
    }
};

}// namespace botix::cli