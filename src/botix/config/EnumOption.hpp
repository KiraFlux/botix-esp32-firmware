// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/Labeled.hpp>

namespace botix::config {

/// @brief Named value of an enumerated configuration field
struct EnumOption : kf::mixin::Labeled {

    constexpr EnumOption(kf::StringView label, kf::usize value) noexcept :
        kf::mixin::Labeled{label}, _value{value} {}

    [[nodiscard]] constexpr kf::usize value() const noexcept {
        return _value;
    }

private:
    kf::usize _value;
};

}// namespace botix::config
