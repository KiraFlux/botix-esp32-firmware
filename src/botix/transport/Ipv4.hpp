// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/primitives.hpp>

#include <kf/mixin/Equatable.hpp>
#include <kf/mixin/Representable.hpp>

#include "botix/Text.hpp"

namespace botix::transport {

/// @brief IPv4 address stored in host byte order (a.b.c.d -> 0xAABBCCDD)
/// @note Trivially copyable, so it can live directly in a persisted config blob
struct Ipv4 :

    kf::mixin::Equatable<Ipv4>,
    kf::mixin::Representable<Ipv4, Text<16>>

{
    kf::u32 value;

    /// @brief Compose from dotted-quad octets
    [[nodiscard]] static constexpr Ipv4 fromOctets(kf::u8 a, kf::u8 b, kf::u8 c, kf::u8 d) noexcept {
        Ipv4 ret{};
        ret.value = (static_cast<kf::u32>(a) << 24) |
                    (static_cast<kf::u32>(b) << 16) |
                    (static_cast<kf::u32>(c) << 8) |
                    static_cast<kf::u32>(d);
        return ret;
    }

    /// @brief Extract octet, 0 is the most significant
    [[nodiscard]] constexpr kf::u8 octet(kf::u8 index) const noexcept {
        return static_cast<kf::u8>(value >> (8 * (3 - index)));
    }

    [[nodiscard]] constexpr bool unset() const noexcept {
        return value == 0;
    }

private:
    KF_IMPL_EQUATABLE(Ipv4);
    constexpr bool isEqualsImpl(Ipv4 const &other) const noexcept {
        return value == other.value;
    }

    KF_IMPL_REPRESENTABLE(Ipv4, Text<16>);
    auto reprImpl() const noexcept {
        return Text<16>::formatted("{}.{}.{}.{}", octet(0), octet(1), octet(2), octet(3));
    }
};

}// namespace botix::transport
