// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/primitives.hpp>

#include <kf/mixin/Equatable.hpp>
#include <kf/mixin/Representable.hpp>

#include "botix/Text.hpp"
#include "botix/transport/Ipv4.hpp"

namespace botix::transport {

/// @brief Address and port of a WiFi peer
struct IpEndpoint :

    kf::mixin::Equatable<IpEndpoint>,
    kf::mixin::Representable<IpEndpoint, Text<24>>

{
    Ipv4 address;
    kf::u16 port;

    /// @brief An unset endpoint, meaning no remote is configured
    [[nodiscard]] constexpr bool empty() const noexcept {
        return address.unset() or port == 0;
    }

private:
    KF_IMPL_EQUATABLE(IpEndpoint);
    constexpr bool isEqualsImpl(IpEndpoint const &other) const noexcept {
        return address == other.address and port == other.port;
    }

    KF_IMPL_REPRESENTABLE(IpEndpoint, Text<24>);
    auto reprImpl() const noexcept {
        return Text<24>::formatted("{}:{}", address, port);
    }
};

}// namespace botix::transport
