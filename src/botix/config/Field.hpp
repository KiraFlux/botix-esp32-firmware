// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Bytes.hpp>
#include <kf/Option.hpp>
#include <kf/Slice.hpp>
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

/// @brief Storage class of a configuration field, selecting how bytes are read and written
enum class FieldKind : kf::u8 {
    Boolean,
    /// @brief Signed integer, width taken from `Field::size`
    Signed,
    /// @brief Unsigned integer, width taken from `Field::size`
    Unsigned,
    /// @brief 32-bit float
    Real,
    /// @brief Unsigned integer restricted to a named option set
    Enumerated,
    /// @brief NUL-terminated character buffer of `Field::size` bytes including terminator
    Text,
    /// @brief 32-bit IPv4 address rendered as dotted quad
    Ipv4,
};

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

    FieldKind kind;

    /// @brief Value is not shown; display is replaced by a placeholder
    bool secret;

    /// @brief Allowed values, for `FieldKind::Enumerated`
    kf::Slice<EnumOption const> options;

    /// @brief Inclusive bounds for numeric kinds; none leaves the side unconstrained
    kf::Option<kf::f64> min, max;
};

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
