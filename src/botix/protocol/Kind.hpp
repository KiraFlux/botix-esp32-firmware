// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/StringView.hpp>

namespace botix::protocol {

enum class Kind : unsigned char {
    Raw = 0x00,
    Mavlink = 0x01,
};

/// @brief Console-facing name of a protocol kind
/// @note Lives with the enum so handlers never spell these out themselves
[[nodiscard]] constexpr kf::StringView name(Kind kind) noexcept {
    switch (kind) {
        case Kind::Raw: return "raw";
        case Kind::Mavlink: return "mavlink";
        default: return "?";
    }
}

}// namespace botix::protocol
