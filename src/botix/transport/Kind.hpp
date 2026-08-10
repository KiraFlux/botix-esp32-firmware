// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/StringView.hpp>

namespace botix::transport {

enum class Kind : unsigned char {
    Espnow = 0x00,
    Wifi = 0x01,
};

/// @brief Console-facing name of a transport kind
/// @note Lives with the enum so handlers never spell these out themselves
[[nodiscard]] constexpr kf::StringView name(Kind kind) noexcept {
    switch (kind) {
        case Kind::Espnow: return "espnow";
        case Kind::Wifi: return "wifi";
        default: return "?";
    }
}

}// namespace botix::transport
