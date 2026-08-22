// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Function.hpp>
#include <kf/Slice.hpp>
#include <kf/core.hpp>

#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/ReprTo.hpp>

#include "botix/cli/Argument.hpp"
#include "botix/cli/Identifier.hpp"

namespace botix::cli {

struct Command :

    Identifier,
    kf::mixin::NonCopyable,
    kf::mixin::ReprTo<Command>

{

    struct Context {
        Channel::Context const &channel;
        kf::Slice<Argument const> arguments;
    };

    explicit constexpr Command(Identifier id, kf::Slice<Argument> arguments, auto &&handler) noexcept :
        Identifier{id},
        _arguments{arguments},
        _handler{std::forward<decltype(handler)>(handler)} {}

    // properties

    [[nodiscard]] constexpr auto arguments() noexcept {
        return _arguments;
    }

    [[nodiscard]] constexpr kf::Slice<Argument const> arguments() const noexcept {
        return _arguments;
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

    // control

    void execute(Channel::Context const &channel_context) const noexcept {
        _handler(Context{
            .channel = channel_context,
            .arguments = arguments(),
        });
    }

private:
    kf::Slice<Argument> _arguments;
    kf::Function<void(Context const &)> _handler;

    KF_IMPL_REPR_TO(Command);
    constexpr kf::usize reprToImpl(auto &char_writable) const noexcept {
        kf::usize write_count = char_writable.append(this->name);

        for (auto const &a: this->arguments()) {
            write_count += char_writable.append(' ');
            write_count += char_writable.append(a);
        }

        return write_count;
    }
};

}// namespace botix::cli