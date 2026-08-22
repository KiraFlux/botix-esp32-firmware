// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <charconv>

#include <kf/Option.hpp>
#include <kf/StringView.hpp>
#include <kf/core.hpp>

namespace botix {

template<typename T> struct Parser;

template<> struct Parser<bool> {

    [[nodiscard]] kf::Option<bool> parse(kf::StringView lexeme) noexcept {
        if (lexeme == "true") {
            return kf::some(true);
        }

        if (lexeme == "false") {
            return kf::some(false);
        }

        return kf::none;
    }
};

template<> struct Parser<char> {

    [[nodiscard]] kf::Option<char> parse(kf::StringView lexeme) noexcept {
        if (lexeme.length() == 1) {
            return kf::some(lexeme[0]);
        }

        return kf::none;
    }
};

template<kf::integer_type T> struct Parser<T> {

    [[nodiscard]] kf::Option<T> parse(kf::StringView lexeme) noexcept {
        auto const match_base = [](kf::StringView const &lexeme) -> kf::usize {
            if (lexeme.length() > 2 and '0' == lexeme[0]) {
                switch (lexeme[1]) {
                    case 'b':
                    case 'B':
                        return 2;

                    case 'x':
                    case 'X':
                        return 16;
                }
            }
            return 10;
        };

        T value;
        auto const base = match_base(lexeme);

        if (base != 10) {
            lexeme = lexeme.fromOffset(2);
        }

        auto const result = std::from_chars(lexeme.begin(), lexeme.end(), value, base);
        if (result.ec == std::errc{} and result.ptr == lexeme.end()) {
            return kf::some(value);
        }
        return kf::none;
    }
};

template<kf::float_type T> struct Parser<T> {

    [[nodiscard]] kf::Option<T> parse(kf::StringView lexeme) noexcept {
        T value;
        auto const result = std::from_chars(lexeme.begin(), lexeme.end(), value);
        if (result.ec == std::errc{} and result.ptr == lexeme.end()) {
            return kf::some(value);
        }
        return kf::none;
    }
};

}// namespace botix