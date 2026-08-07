// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/MacAddress.hpp>

#include <kf/mixin/Equatable.hpp>

#include "botix/transport/Kind.hpp"

namespace botix::transport {

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

    [[nodiscard]] constexpr Kind kind() const noexcept {
        return _kind;
    }

    [[nodiscard]] constexpr kf::MacAddress const &mac() const noexcept {
        return _espnow_mac_address;
    }

private:
    Kind _kind;

    union {
        kf::MacAddress _espnow_mac_address{};
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
                // TODO: implement this
                return false;

            default:
                return false;
        }
    }
};

}// namespace botix::transport