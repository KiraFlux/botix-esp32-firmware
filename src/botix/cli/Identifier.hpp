// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Option.hpp>
#include <kf/StringView.hpp>

namespace botix::cli {

// TODO: impl ReprTo for Identifier
struct Identifier {

    kf::StringView name;
    kf::Option<char> shortcut{name.empty() ? kf::none : kf::some(name[0])};

    [[nodiscard]] constexpr bool match(kf::StringView name_or_shortcut) const noexcept {
        return (name == name_or_shortcut) or (name_or_shortcut.length() == 1 and shortcut.unwrapOr(0) == name_or_shortcut[0]);
    }
};

}// namespace botix::cli