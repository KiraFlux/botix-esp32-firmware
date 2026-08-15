// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/Function.hpp>
#include <kf/Registry.hpp>
#include <kf/Slice.hpp>
#include <kf/Stack.hpp>
#include <kf/core.hpp>

#include <kf/mixin/ExtraAllocationLength.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/ReprTo.hpp>

#include "botix/cli/Argument.hpp"
#include "botix/cli/Identifier.hpp"

namespace botix::cli {

struct Command :

    Identifier,
    private kf::Registry<Argument>,
    kf::mixin::NonCopyable,
    kf::mixin::ExtraAllocationLength<Command>,
    kf::mixin::ReprTo<Command>

{
    struct Context {
        Channel::Context const &channel;
        kf::Slice<Argument *const> arguments;
    };

    explicit constexpr Command(kf::Arena &arena, Config const &config, Identifier id, auto &&handler) noexcept :
        Identifier{id},
        kf::Registry<Argument>{arena, config.max_command_argument_count},
        _handler{std::forward<decltype(handler)>(handler)} {}

    // properties

    [[nodiscard]] constexpr auto arguments() noexcept {
        return this->items();
    }

    [[nodiscard]] constexpr auto arguments() const noexcept {
        return this->items();
    }

    [[nodiscard]] constexpr kf::usize positionalArgumentsCount() const noexcept {
        kf::usize count = 0;
        for (auto const a: this->items()) {
            count += static_cast<kf::usize>(a->positional());
        }
        return count;
    }

    // argument
    // TODO: check if default out of constraint
    // TODO: check if non-default after default
    // TODO: check for name arg collision

    [[nodiscard]] bool addEnumArgument(kf::Arena &arena, Identifier id, Argument::EnumParameters params) noexcept {
        return this->add(arena, id, params).isSome();
    }

    [[nodiscard]] bool addBooleanArgument(kf::Arena &arena, Identifier id, Argument::BooleanParameters params) noexcept {
        return this->add(arena, id, params).isSome();
    }

    [[nodiscard]] bool addIntegerArgument(kf::Arena &arena, Identifier id, Argument::IntegerParameters params) noexcept {
        return this->add(arena, id, params).isSome();
    }

    [[nodiscard]] bool addRealArgument(kf::Arena &arena, Identifier id, Argument::RealParameters params) noexcept {
        return this->add(arena, id, params).isSome();
    }

    [[nodiscard]] bool addStringArgument(kf::Arena &arena, Identifier id, Argument::StringParameters params) noexcept {
        return this->add(arena, id, params).isSome();
    }

    // control

    void execute(Channel::Context const &channel_context) const noexcept {
        _handler(Context{
            .channel = channel_context,
            .arguments = arguments(),
        });
    }

private:
    kf::Function<void(Context const &)> _handler;

    KF_IMPL_EXTRA_ALLOCATION_LENGTH(Command);
    static constexpr auto getExtraAllocationLengthImpl(Config const &config, auto const &...args) noexcept {
        return static_cast<kf::usize>(config.max_command_argument_count * sizeof(Argument));
    }

    KF_IMPL_REPR_TO(Command);
    constexpr kf::usize reprToImpl(auto &char_writable) const noexcept {
        kf::usize write_count = char_writable.append(this->name);

        for (auto a: this->arguments()) {
            write_count += char_writable.append(' ');
            write_count += char_writable.append(*a);
        }

        return write_count;
    }
};

}// namespace botix::cli