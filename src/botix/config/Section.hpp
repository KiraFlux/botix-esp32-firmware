// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Bytes.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/Slice.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include "botix/config/Field.hpp"

namespace botix::config {

/// @brief A contiguous group of fields backed by one persisted config blob
struct Section {

    /// @brief Section name, the first component of a fully qualified path
    kf::StringView name;

    /// @brief Raw bytes of the config struct the offsets apply to
    kf::Bytes bytes;

    kf::Slice<Field const> fields;

    /// @brief Locate a field by its section-relative path
    [[nodiscard]] constexpr auto find(kf::StringView path) const noexcept -> kf::Option<Field const &> {
        for (auto const &field: fields) {
            if (field.path == path) {
                return kf::someRef(field);
            }
        }
        return kf::none;
    }

    /// @brief Address of a field's value within this section's blob
    /// @note `bytes` is a non-owning view, so a copy still yields mutable access
    [[nodiscard]] constexpr kf::u8 *at(Field const &field) const noexcept {
        kf::Bytes view = bytes;
        return view.data() + field.offset;
    }
};

}// namespace botix::config
