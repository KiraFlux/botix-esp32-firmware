// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/Logger.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/Registry.hpp>
#include <kf/StringView.hpp>
#include <kf/core.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/ExtraAllocationLength.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

#include "botix/cli/Channel.hpp"
#include "botix/cli/Config.hpp"
#include "botix/cli/Identifier.hpp"
#include "botix/cli/Namespace.hpp"

namespace botix::internal {

using ChannelContainerBase = kf::Registry<cli::Channel>;

template<typename Impl> struct ChannelContainer : private ChannelContainerBase {

    using ChannelContainerBase::ChannelContainerBase;

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

using ConsoleNamespaceContainerBase = kf::Registry<cli::Namespace>;

template<typename Impl> struct ConsoleNamespaceContainer : private ConsoleNamespaceContainerBase {

    explicit ConsoleNamespaceContainer(kf::Arena &arena, kf::usize max_namespace_count) noexcept :
        ConsoleNamespaceContainerBase{arena, max_namespace_count} {
        (void) this->addNamespace(arena, {.name = "global", .shortcut = kf::none});// caller ensure
    }

    [[nodiscard]] decltype(auto) namespaces() noexcept {
        return this->items();
    }

    [[nodiscard]] decltype(auto) namespaces() const noexcept {
        return this->items();
    }

    [[nodiscard]] decltype(auto) globalNamespace() noexcept {
        return *namespaces()[0];
    }

    [[nodiscard]] decltype(auto) globalNamespace() const noexcept {
        return *namespaces()[0];
    }

    [[nodiscard]] decltype(auto) getNamespace(kf::StringView name_or_shortcut) noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] decltype(auto) getNamespace(kf::StringView name_or_shortcut) const noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] decltype(auto) addNamespace(kf::Arena &arena, cli::Identifier id) {
        return this->add(arena, static_cast<Impl const *>(this)->config(), id);
    }
};

}// namespace botix::internal

namespace botix::cli {

struct Console final :

    kf::mixin::Configured<Config>,
    kf::mixin::TimedPollable<Console>,
    kf::mixin::ExtraAllocationLength<Console>,
    internal::ChannelContainer<Console>,
    internal::ConsoleNamespaceContainer<Console>

{

    explicit Console(kf::Arena &arena, Config const &config) noexcept :
        kf::mixin::Configured<Config>{config},
        internal::ChannelContainer<Console>{arena, config.max_channel_count},
        internal::ConsoleNamespaceContainer<Console>{arena, config.max_namespace_count} {

        // TODO: register help command
    }

    [[nodiscard]] auto resolveCommand(kf::StringView path) noexcept -> kf::Option<Command &> {
        auto const maybe_delimeter_index = path.indexOf('.');

        if (maybe_delimeter_index.isNone()) {
            return globalNamespace().getCommand(path);
        }

        auto const delimeter_index = maybe_delimeter_index.unwrap();

        auto namespace_ = getNamespace(path.first(delimeter_index));
        if (namespace_.isNone()) { return kf::none; }

        return namespace_.unwrap().getCommand(path.fromOffset(delimeter_index + 1));
    }

private:
    kf::Logger _logger{"Console"};

    void onInputLineReady(Channel::Context const &channel_context) noexcept {
        if (channel_context.parameters.echo) {
            channel_context.output.print("[#{}]>>> {}", channel_context.num, channel_context.input_line);
        }

        kf::StringView tokens_buffer[this->config().max_command_argument_count]{};

        auto tokens = channel_context.input_line.trim().split({tokens_buffer, this->config().max_command_argument_count});

        if (tokens.empty()) {
            return;
        }

        auto const name = tokens[0];
        auto maybe_command = resolveCommand(name);

        if (maybe_command.isNone()) {
            channel_context.output.error("unknown command: {}", name);
            return;
        }

        auto &command = maybe_command.unwrap();
        auto argument_tokens = tokens.fromOffset(1);

        if (argument_tokens.length() < command.positionalArgumentsCount() or argument_tokens.length() > command.arguments().length()) {
            channel_context.output.error("expected {}..{} arguments, got {}", command.positionalArgumentsCount(), command.arguments().length(), argument_tokens.length());
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
            return;
        }

        auto default_value_arguments = command.arguments().fromOffset(argument_index);
        for (auto &a: default_value_arguments) {
            a.reset();
        }

        command.execute({
            .channel = channel_context,
            .arguments = command.arguments(),
        });
    }

    KF_IMPL_TIMED_POLLABLE(Console);
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
        return static_cast<kf::usize>(config.max_channel_count * sizeof(Channel) + config.max_namespace_count * sizeof(Namespace));
    }
};

}// namespace botix::cli