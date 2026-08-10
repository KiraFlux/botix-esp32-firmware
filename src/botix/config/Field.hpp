// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Option.hpp>
#include <kf/Slice.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include "botix/config/EnumOption.hpp"
#include "botix/config/Kind.hpp"

namespace botix::config {

/// @brief One addressable configuration field, described by its offset within a POD config
/// @note Holds no pointer to the value: the owning `Section` supplies the base bytes,
///       which keeps the whole table constant and shareable.
struct Field {

    /// @brief Dotted path used from the console, unique within a section
    kf::StringView path;

    /// @brief Byte offset of the value from the start of the config struct
    kf::u16 offset;

    /// @brief Width of the stored value in bytes
    kf::u16 size;

    Kind kind;

    /// @brief Value is not shown; display is replaced by a placeholder
    bool secret;

    /// @brief Allowed values, for `Kind::Enumerated`
    kf::Slice<EnumOption const> options;

    /// @brief Inclusive bounds for numeric kinds; none leaves the side unconstrained
    kf::Option<kf::f64> min, max;

    /// @brief Whether every field addresses bytes that belong to its config struct
    /// @note Meant for a `static_assert` on a field table: a stale offset would
    ///       otherwise silently read and write a neighbouring field.
    template<kf::usize N> [[nodiscard]] static constexpr bool allWithin(Field const (&fields)[N], kf::usize config_size) noexcept {
        for (auto const &field: fields) {
            if (kf::usize{field.offset} + kf::usize{field.size} > config_size) {
                return false;
            }
        }
        return true;
    }

    /// @brief Whether every path in a table is distinct
    /// @note Paths are the console's addressing scheme, so duplicates would
    ///       make one of the fields unreachable.
    template<kf::usize N> [[nodiscard]] static constexpr bool pathsUnique(Field const (&fields)[N]) noexcept {
        for (kf::usize i = 0; i < N; i += 1) {
            for (kf::usize j = i + 1; j < N; j += 1) {
                if (fields[i].path == fields[j].path) {
                    return false;
                }
            }
        }
        return true;
    }
};

}// namespace botix::config
