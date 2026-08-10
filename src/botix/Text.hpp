// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Sequence.hpp>
#include <kf/String.hpp>
#include <kf/StringView.hpp>
#include <kf/primitives.hpp>

namespace botix {

/// @brief Owning fixed-capacity string that reports its real length
/// @tparam N Capacity in characters
/// @note `kf::Array<char, N>` would also satisfy the char writer, but it always
///       reports `N`, so a shorter value trails padding bytes into the output.
///       This carries the formatted length instead, which makes it safe to
///       return from `reprImpl()` for values of varying width.
template<kf::usize N> struct Text : kf::Sequence<Text<N>, char> {

    using Self = Text<N>;

    constexpr Text() noexcept = default;

    /// @brief Build from a format string, truncating at capacity
    template<typename... Args> [[nodiscard]] static auto formatted(kf::internal::FormatString<Args...> const &fmt, Args const &...args) noexcept -> Self {
        Self ret{};

        kf::String builder{{ret._data, N}};
        builder.format(fmt, args...);
        ret._length = builder.length();

        return ret;
    }

    [[nodiscard]] constexpr kf::StringView view() const noexcept {
        return {_data, _length};
    }

private:
    char _data[N]{};
    kf::usize _length{0};

    KF_IMPL_SEQUENCE(Self, char);

    constexpr char *getDataImpl() noexcept {
        return _data;
    }

    constexpr kf::usize lengthImpl() const noexcept {
        return _length;
    }
};

}// namespace botix
