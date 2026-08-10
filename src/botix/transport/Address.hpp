// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/MacAddress.hpp>

#include <kf/mixin/Equatable.hpp>
#include <kf/mixin/Representable.hpp>

#include "botix/Text.hpp"
#include "botix/transport/IpEndpoint.hpp"
#include "botix/transport/Kind.hpp"

namespace botix::transport {

struct Address :

    kf::mixin::Equatable<Address>,
    kf::mixin::Representable<Address, Text<32>>

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

    KF_IMPL_REPRESENTABLE(Self, Text<32>);
    auto reprImpl() const noexcept {
        switch (_kind) {
            case Kind::Wifi:
                return Text<32>::formatted("{}", _wifi_endpoint);

            case Kind::Espnow:
                return Text<32>::formatted("{}", _espnow_mac_address);

            default:
                return Text<32>::formatted("?");
        }
    }
};

}// namespace botix::transport
