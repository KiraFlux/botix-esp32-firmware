// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

namespace botix::config {

/// @brief Outcome of writing a lexeme into a configuration field
enum class SetStatus : kf::u8 {
    Ok,
    /// @brief The lexeme is not a valid literal for this field kind
    Malformed,
    /// @brief Parsed, but outside the field's declared bounds
    OutOfRange,
    /// @brief Text longer than the field can store
    TooLong,
    /// @brief Not one of the field's named options
    UnknownOption,
};

/// @brief Console-facing description of a write outcome
/// @note Lives with the enum so handlers never spell these out themselves
[[nodiscard]] constexpr kf::StringView name(SetStatus status) noexcept {
    switch (status) {
        case SetStatus::Ok: return "ok";
        case SetStatus::Malformed: return "malformed value";
        case SetStatus::OutOfRange: return "out of range";
        case SetStatus::TooLong: return "too long";
        case SetStatus::UnknownOption: return "unknown option";
        default: return "?";
    }
}

}// namespace botix::config
