// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>// For std::forward, std::move

#include <kf/Arena.hpp>
#include <kf/Registry.hpp>
#include <kf/StringView.hpp>
#include <kf/core.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/ExtraAllocationLength.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/cli/Command.hpp"
#include "botix/cli/Config.hpp"
#include "botix/cli/Identifier.hpp"

namespace botix::cli {

struct Group :

    Identifier,
    private kf::Registry<Command>,
    kf::mixin::NonCopyable,
    kf::mixin::Configured<Config>,
    kf::mixin::ExtraAllocationLength<Group>

{

    explicit constexpr Group(kf::Arena &arena, Config const &config, Identifier id) noexcept :
        kf::mixin::Configured<Config>{config},
        Identifier{id},
        kf::Registry<Command>{arena, config.max_command_count} {}

    [[nodiscard]] constexpr auto commands() noexcept {
        return this->items();
    }

    [[nodiscard]] constexpr auto commands() const noexcept {
        return this->items();
    }

    [[nodiscard]] auto getCommand(kf::StringView name_or_shortcut) noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] auto getCommand(kf::StringView name_or_shortcut) const noexcept {
        return this->get(name_or_shortcut);
    }

    [[nodiscard]] auto addCommand(kf::Arena &arena, Identifier id, auto &&handler) noexcept {
        return this->add(arena, this->config(), std::move(id), std::forward<decltype(handler)>(handler));
    }

private:
    KF_IMPL_EXTRA_ALLOCATION_LENGTH(Group);

    static constexpr auto getExtraAllocationLengthImpl(Config const &config, auto const &...args) noexcept {
        return static_cast<kf::usize>(config.max_command_count * sizeof(Command));
    }
};

}// namespace botix::cli