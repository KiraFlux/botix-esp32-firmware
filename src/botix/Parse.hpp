// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

namespace botix {

/// @brief Lexeme parsers shared by the console and the configuration registry
struct Parse {

    /// @brief Parse a decimal or `0x`-prefixed integer
    /// @return None when the lexeme is malformed or the magnitude overflows
    [[nodiscard]] static constexpr auto integer(kf::StringView lexeme) noexcept -> kf::Option<kf::i64> {
        if (lexeme.empty()) {
            return kf::none;
        }

        kf::usize i = 0;
        bool negative = false;

        if (lexeme[0] == '+' or lexeme[0] == '-') {
            negative = lexeme[0] == '-';
            i = 1;
        }

        kf::u8 base = 10;

        if (lexeme.length() - i > 2 and lexeme[i] == '0' and (lexeme[i + 1] == 'x' or lexeme[i + 1] == 'X')) {
            base = 16;
            i += 2;
        }

        if (i >= lexeme.length()) {
            return kf::none;// sign or prefix with no digits
        }

        // Bound the magnitude so that both i64 extremes remain representable
        constexpr kf::u64 magnitude_limit{9223372036854775807ull};

        kf::u64 magnitude = 0;

        for (; i < lexeme.length(); i += 1) {
            char const c = lexeme[i];

            kf::u8 digit;

            if (isDigit(c)) {
                digit = static_cast<kf::u8>(c - '0');
            } else if (base == 16 and c >= 'a' and c <= 'f') {
                digit = static_cast<kf::u8>(c - 'a' + 10);
            } else if (base == 16 and c >= 'A' and c <= 'F') {
                digit = static_cast<kf::u8>(c - 'A' + 10);
            } else {
                return kf::none;// trailing garbage
            }

            if (magnitude > (magnitude_limit - digit) / base) {
                return kf::none;// overflow
            }

            magnitude = magnitude * base + digit;
        }

        auto const value = static_cast<kf::i64>(magnitude);

        return kf::some(negative ? -value : value);
    }

    /// @brief Parse a decimal real number: sign, integer part, optional fraction
    /// @note Exponent notation is deliberately unsupported; configuration values do not need it
    [[nodiscard]] static constexpr auto real(kf::StringView lexeme) noexcept -> kf::Option<kf::f32> {
        if (lexeme.empty()) {
            return kf::none;
        }

        kf::usize i = 0;
        bool negative = false;

        if (lexeme[0] == '+' or lexeme[0] == '-') {
            negative = lexeme[0] == '-';
            i = 1;
        }

        kf::f64 value = 0;
        bool any_digit = false;

        for (; i < lexeme.length() and isDigit(lexeme[i]); i += 1) {
            value = value * 10 + static_cast<kf::f64>(lexeme[i] - '0');
            any_digit = true;
        }

        if (i < lexeme.length() and lexeme[i] == '.') {
            i += 1;

            kf::f64 scale = 0.1;

            for (; i < lexeme.length() and isDigit(lexeme[i]); i += 1) {
                value += scale * static_cast<kf::f64>(lexeme[i] - '0');
                scale *= 0.1;
                any_digit = true;
            }
        }

        if (not any_digit or i != lexeme.length()) {
            return kf::none;// no digits, or trailing garbage
        }

        return kf::some(static_cast<kf::f32>(negative ? -value : value));
    }

    /// @brief Parse a dotted-quad IPv4 address into host byte order
    [[nodiscard]] static constexpr auto ipv4(kf::StringView lexeme) noexcept -> kf::Option<kf::u32> {
        kf::u32 address = 0;
        kf::usize i = 0;

        for (kf::u8 octet_index = 0; octet_index < 4; octet_index += 1) {
            if (octet_index > 0) {
                if (i >= lexeme.length() or lexeme[i] != '.') {
                    return kf::none;
                }
                i += 1;
            }

            if (i >= lexeme.length() or not isDigit(lexeme[i])) {
                return kf::none;
            }

            kf::u32 octet = 0;
            kf::u8 digits = 0;

            while (i < lexeme.length() and isDigit(lexeme[i]) and digits < 3) {
                octet = octet * 10 + static_cast<kf::u32>(lexeme[i] - '0');
                digits += 1;
                i += 1;
            }

            if (octet > 255) {
                return kf::none;
            }

            address = (address << 8) | octet;
        }

        if (i != lexeme.length()) {
            return kf::none;// trailing garbage
        }

        return kf::some(address);
    }

private:
    [[nodiscard]] static constexpr bool isDigit(char c) noexcept {
        return c >= '0' and c <= '9';
    }
};

}// namespace botix
