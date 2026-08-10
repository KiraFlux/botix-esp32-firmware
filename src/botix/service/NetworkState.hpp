// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

namespace botix::service {

/// @brief Stage of the WiFi station link
enum class NetworkState : kf::u8 {
    /// @brief Station disabled by configuration
    Disabled,
    /// @brief Association in progress
    Connecting,
    /// @brief Associated, address acquired, mDNS published
    Connected,
};

/// @brief Console-facing name of a network state
/// @note Lives with the enum so handlers never spell these out themselves
[[nodiscard]] constexpr kf::StringView name(NetworkState state) noexcept {
    switch (state) {
        case NetworkState::Disabled: return "disabled";
        case NetworkState::Connecting: return "connecting";
        case NetworkState::Connected: return "connected";
        default: return "?";
    }
}

}// namespace botix::service
