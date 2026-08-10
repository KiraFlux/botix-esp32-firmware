// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/MacAddress.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/Equatable.hpp>

#include "botix/transport/Kind.hpp"

namespace botix::transport {

/// @brief IPv4 endpoint of a WiFi peer
struct IpEndpoint : kf::mixin::Equatable<IpEndpoint> {

    /// @brief IPv4 address in host byte order (a.b.c.d -> 0xAABBCCDD)
    kf::u32 address;

    kf::u16 port;

    /// @brief Compose from dotted-quad octets
    [[nodiscard]] static constexpr kf::u32 pack(kf::u8 a, kf::u8 b, kf::u8 c, kf::u8 d) noexcept {
        return (static_cast<kf::u32>(a) << 24) |
               (static_cast<kf::u32>(b) << 16) |
               (static_cast<kf::u32>(c) << 8) |
               static_cast<kf::u32>(d);
    }

    /// @brief Extract octet, 0 is the most significant
    [[nodiscard]] constexpr kf::u8 octet(kf::u8 index) const noexcept {
        return static_cast<kf::u8>(address >> (8 * (3 - index)));
    }

    /// @brief An unset endpoint, used to mean "no remote configured"
    [[nodiscard]] constexpr bool empty() const noexcept {
        return address == 0 or port == 0;
    }

private:
    KF_IMPL_EQUATABLE(IpEndpoint);
    constexpr bool isEqualsImpl(IpEndpoint const &other) const noexcept {
        return address == other.address and port == other.port;
    }
};

struct Address :

    kf::mixin::Equatable<Address>

{
    using Self = Address;

    [[nodiscard]] static constexpr Self createForEspnow(kf::MacAddress const &mac_address) noexcept {
        Self ret{};
        ret._kind = Kind::Espnow;
        ret._espnow_mac_address = mac_address;
        return ret;
    }

    [[nodiscard]] static constexpr Self createForWifi(IpEndpoint const &endpoint) noexcept {
        Self ret{};
        ret._kind = Kind::Wifi;
        ret._wifi_endpoint = endpoint;
        return ret;
    }

    [[nodiscard]] constexpr Kind kind() const noexcept {
        return _kind;
    }

    [[nodiscard]] constexpr kf::MacAddress const &mac() const noexcept {
        return _espnow_mac_address;
    }

    [[nodiscard]] constexpr IpEndpoint const &endpoint() const noexcept {
        return _wifi_endpoint;
    }

private:
    Kind _kind;

    union {
        kf::MacAddress _espnow_mac_address{};
        IpEndpoint _wifi_endpoint;
    };

    KF_IMPL_EQUATABLE(Self);
    constexpr bool isEqualsImpl(Self const &other) const noexcept {
        if (other._kind != _kind) {
            return false;
        }

        switch (_kind) {
            case Kind::Espnow:
                return _espnow_mac_address == other._espnow_mac_address;

            case Kind::Wifi:
                return _wifi_endpoint == other._wifi_endpoint;

            default:
                return false;
        }
    }
};

}// namespace botix::transport
