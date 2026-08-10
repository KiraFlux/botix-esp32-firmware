// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/primitives.hpp>

namespace botix::config {

/// @brief Storage class of a configuration field, selecting how bytes are read and written
enum class Kind : kf::u8 {
    Boolean,
    /// @brief Signed integer, width taken from `Field::size`
    Signed,
    /// @brief Unsigned integer, width taken from `Field::size`
    Unsigned,
    /// @brief Floating point, width taken from `Field::size`
    Real,
    /// @brief Unsigned integer restricted to a named option set
    Enumerated,
    /// @brief NUL-terminated character buffer of `Field::size` bytes including terminator
    Text,
    /// @brief 32-bit IPv4 address rendered as a dotted quad
    Ipv4,
};

}// namespace botix::config
