// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

#include "botix/Parse.hpp"
#include "botix/config/Field.hpp"

/// @brief Typed reads and writes of configuration fields located by byte offset
namespace botix::config::access {

/// @brief Copy a value out of the blob byte by byte
/// @note Avoids forming an unaligned pointer, which traps on xtensa for wide loads
template<typename T> [[nodiscard]] inline T load(kf::u8 const *source) noexcept {
    T value{};
    auto *destination = reinterpret_cast<kf::u8 *>(&value);

    for (kf::usize i = 0; i < sizeof(T); i += 1) {
        destination[i] = source[i];
    }

    return value;
}

template<typename T> inline void store(kf::u8 *destination, T value) noexcept {
    auto const *source = reinterpret_cast<kf::u8 const *>(&value);

    for (kf::usize i = 0; i < sizeof(T); i += 1) {
        destination[i] = source[i];
    }
}

[[nodiscard]] inline bool readBoolean(Section const &section, Field const &field) noexcept {
    return load<kf::u8>(section.at(field)) != 0;
}

[[nodiscard]] inline kf::i64 readSigned(Section const &section, Field const &field) noexcept {
    auto const *p = section.at(field);

    switch (field.size) {
        case 1: return load<kf::i8>(p);
        case 2: return load<kf::i16>(p);
        case 4: return load<kf::i32>(p);
        case 8: return load<kf::i64>(p);
        default: return 0;
    }
}

[[nodiscard]] inline kf::u64 readUnsigned(Section const &section, Field const &field) noexcept {
    auto const *p = section.at(field);

    switch (field.size) {
        case 1: return load<kf::u8>(p);
        case 2: return load<kf::u16>(p);
        case 4: return load<kf::u32>(p);
        case 8: return load<kf::u64>(p);
        default: return 0;
    }
}

/// @note Width follows `Field::size`: some units, such as `Millimeters`, are 64-bit
[[nodiscard]] inline kf::f64 readReal(Section const &section, Field const &field) noexcept {
    return field.size == sizeof(kf::f64)
               ? load<kf::f64>(section.at(field))
               : static_cast<kf::f64>(load<kf::f32>(section.at(field)));
}

[[nodiscard]] inline kf::u32 readIpv4(Section const &section, Field const &field) noexcept {
    return load<kf::u32>(section.at(field));
}

/// @brief View of a text field, stopping at the terminator
[[nodiscard]] inline kf::StringView readText(Section const &section, Field const &field) noexcept {
    auto const *p = reinterpret_cast<char const *>(section.at(field));

    kf::usize length = 0;
    while (length + 1 < field.size and p[length] != '\0') {
        length += 1;
    }

    return {p, length};
}

/// @brief Name of the option currently selected by an enumerated field
[[nodiscard]] inline kf::StringView readOptionName(Section const &section, Field const &field) noexcept {
    auto const current = readUnsigned(section, field);

    for (auto const &option: field.options) {
        if (option.value() == current) {
            return option.label();
        }
    }

    return "?";
}

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

[[nodiscard]] constexpr kf::StringView statusName(SetStatus status) noexcept {
    switch (status) {
        case SetStatus::Ok: return "ok";
        case SetStatus::Malformed: return "malformed value";
        case SetStatus::OutOfRange: return "out of range";
        case SetStatus::TooLong: return "too long";
        case SetStatus::UnknownOption: return "unknown option";
        default: return "?";
    }
}

namespace internal {

[[nodiscard]] inline bool withinBounds(Field const &field, kf::f64 value) noexcept {
    if (field.min.isSome() and value < field.min.unwrap()) {
        return false;
    }

    if (field.max.isSome() and value > field.max.unwrap()) {
        return false;
    }

    return true;
}

inline void storeUnsigned(kf::u8 *destination, kf::u16 size, kf::u64 value) noexcept {
    switch (size) {
        case 1: store<kf::u8>(destination, static_cast<kf::u8>(value)); return;
        case 2: store<kf::u16>(destination, static_cast<kf::u16>(value)); return;
        case 4: store<kf::u32>(destination, static_cast<kf::u32>(value)); return;
        case 8: store<kf::u64>(destination, value); return;
        default: return;
    }
}

inline void storeSigned(kf::u8 *destination, kf::u16 size, kf::i64 value) noexcept {
    switch (size) {
        case 1: store<kf::i8>(destination, static_cast<kf::i8>(value)); return;
        case 2: store<kf::i16>(destination, static_cast<kf::i16>(value)); return;
        case 4: store<kf::i32>(destination, static_cast<kf::i32>(value)); return;
        case 8: store<kf::i64>(destination, value); return;
        default: return;
    }
}

/// @brief Widest magnitude representable by an integer field of the given width
[[nodiscard]] constexpr kf::f64 signedLimit(kf::u16 size, bool low) noexcept {
    switch (size) {
        case 1: return low ? -128.0 : 127.0;
        case 2: return low ? -32768.0 : 32767.0;
        case 4: return low ? -2147483648.0 : 2147483647.0;
        default: return low ? -9223372036854775808.0 : 9223372036854775807.0;
    }
}

[[nodiscard]] constexpr kf::f64 unsignedLimit(kf::u16 size) noexcept {
    switch (size) {
        case 1: return 255.0;
        case 2: return 65535.0;
        case 4: return 4294967295.0;
        default: return 18446744073709551615.0;
    }
}

}// namespace internal

/// @brief Parse a lexeme and store it into the field
[[nodiscard]] inline SetStatus set(Section const &section, Field const &field, kf::StringView lexeme) noexcept {
    auto *destination = section.at(field);

    switch (field.kind) {
        case FieldKind::Boolean: {
            bool value;

            if (lexeme == "true" or lexeme == "yes" or lexeme == "y" or lexeme == "1") {
                value = true;
            } else if (lexeme == "false" or lexeme == "no" or lexeme == "n" or lexeme == "0") {
                value = false;
            } else {
                return SetStatus::Malformed;
            }

            store<kf::u8>(destination, static_cast<kf::u8>(value));
            return SetStatus::Ok;
        }

        case FieldKind::Signed: {
            auto const parsed = parse::integer(lexeme);
            if (parsed.isNone()) {
                return SetStatus::Malformed;
            }

            auto const value = static_cast<kf::f64>(parsed.unwrap());

            if (value < internal::signedLimit(field.size, true) or value > internal::signedLimit(field.size, false)) {
                return SetStatus::OutOfRange;
            }

            if (not internal::withinBounds(field, value)) {
                return SetStatus::OutOfRange;
            }

            internal::storeSigned(destination, field.size, parsed.unwrap());
            return SetStatus::Ok;
        }

        case FieldKind::Unsigned: {
            auto const parsed = parse::integer(lexeme);
            if (parsed.isNone()) {
                return SetStatus::Malformed;
            }

            if (parsed.unwrap() < 0) {
                return SetStatus::OutOfRange;
            }

            auto const value = static_cast<kf::f64>(parsed.unwrap());

            if (value > internal::unsignedLimit(field.size) or not internal::withinBounds(field, value)) {
                return SetStatus::OutOfRange;
            }

            internal::storeUnsigned(destination, field.size, static_cast<kf::u64>(parsed.unwrap()));
            return SetStatus::Ok;
        }

        case FieldKind::Real: {
            auto const parsed = parse::real(lexeme);
            if (parsed.isNone()) {
                return SetStatus::Malformed;
            }

            auto const value = static_cast<kf::f64>(parsed.unwrap());

            if (not internal::withinBounds(field, value)) {
                return SetStatus::OutOfRange;
            }

            if (field.size == sizeof(kf::f64)) {
                store<kf::f64>(destination, value);
            } else {
                store<kf::f32>(destination, parsed.unwrap());
            }

            return SetStatus::Ok;
        }

        case FieldKind::Enumerated: {
            for (auto const &option: field.options) {
                if (option.label() == lexeme) {
                    internal::storeUnsigned(destination, field.size, option.value());
                    return SetStatus::Ok;
                }
            }

            return SetStatus::UnknownOption;
        }

        case FieldKind::Text: {
            // Surrounding quotes are stripped, which is also the only way to
            // express an empty value: the console tokenizer drops empty lexemes.
            auto value = lexeme;

            if (value.length() >= 2) {
                auto const first = value[0];
                auto const last = value[value.length() - 1];

                if ((first == '"' or first == '\'') and first == last) {
                    value = value.sub(1, kf::some(value.length() - 2));
                }
            }

            // One byte of the field is reserved for the terminator
            if (value.length() + 1 > field.size) {
                return SetStatus::TooLong;
            }

            auto *text = reinterpret_cast<char *>(destination);

            for (kf::usize i = 0; i < value.length(); i += 1) {
                text[i] = value[i];
            }

            text[value.length()] = '\0';
            return SetStatus::Ok;
        }

        case FieldKind::Ipv4: {
            auto const parsed = parse::ipv4(lexeme);
            if (parsed.isNone()) {
                return SetStatus::Malformed;
            }

            store<kf::u32>(destination, parsed.unwrap());
            return SetStatus::Ok;
        }

        default:
            return SetStatus::Malformed;
    }
}

}// namespace botix::config::access
