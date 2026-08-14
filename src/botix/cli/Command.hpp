// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/Function.hpp>
#include <kf/Slice.hpp>
#include <kf/Stack.hpp>
#include <kf/core.hpp>

#include <kf/mixin/ExtraAllocationLength.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/cli/Argument.hpp"
#include "botix/cli/Identifier.hpp"

namespace botix::cli {

// TODO: use NamedItemContainer
struct Command :

    Identifier,
    kf::mixin::NonCopyable,
    kf::mixin::ExtraAllocationLength<Command>

{
    struct Context {
        Channel::Context const &channel;
        kf::Slice<Argument const> arguments;
    };

    explicit constexpr Command(kf::Arena &arena, Config const &config, Identifier id, auto &&handler) noexcept :
        Identifier{id},
        _arguments{arena.allocate<Argument>(config.max_command_argument_count)},
        _handler{std::forward<decltype(handler)>(handler)} {}

    // properties

    [[nodiscard]] constexpr auto arguments() noexcept {
        return _arguments.slice();
    }

    [[nodiscard]] constexpr auto arguments() const noexcept {
        return _arguments.slice();
    }

    [[nodiscard]] constexpr kf::usize positionalArgumentsCount() const noexcept {
        kf::usize count = 0;
        for (auto const &a: _arguments) {
            count += static_cast<kf::usize>(a.positional());
        }
        return count;
    }

    // argument
    // TODO: check if default out of constraint
    // TODO: check if non-default after default
    // TODO: check for name arg collision

    [[nodiscard]] constexpr bool addEnumArgument(Identifier id, Argument::EnumParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addBooleanArgument(Identifier id, Argument::BooleanParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addIntegerArgument(Identifier id, Argument::IntegerParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addRealArgument(Identifier id, Argument::RealParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    [[nodiscard]] constexpr bool addStringArgument(Identifier id, Argument::StringParameters params) noexcept {
        return _arguments.write(Argument{id, params});
    }

    // control

    void execute(Context const &context) const noexcept {
        _handler(context);
    }

private:
    kf::Stack<Argument> _arguments;
    kf::Function<void(Context const &)> _handler;

    KF_IMPL_EXTRA_ALLOCATION_LENGTH(Command);
    static constexpr auto getExtraAllocationLengthImpl(Config const &config, auto const &...args) noexcept {
        return static_cast<kf::usize>(config.max_command_argument_count * sizeof(Argument));
    }
};

}// namespace botix::cli