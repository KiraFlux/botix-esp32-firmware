// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/transport/EspnowTransport.hpp"
#include "botix/transport/Kind.hpp"
#include "botix/transport/Transport.hpp"
#include "botix/transport/WifiUdpTransport.hpp"

namespace botix::internal {

struct TransportRegistryConfig : kf::mixin::Resettable<TransportRegistryConfig> {

    transport::WifiUdpTransport::Config wifi_udp;

private:
    KF_IMPL_RESETTABLE(TransportRegistryConfig);
    constexpr void resetImpl() noexcept {
        wifi_udp.reset();
    }
};

}// namespace botix::internal

namespace botix::transport {

struct Registry :

    kf::mixin::NonCopyable,
    kf::mixin::Configured<internal::TransportRegistryConfig>

{
    using Config = internal::TransportRegistryConfig;

    using kf::mixin::Configured<Config>::Configured;

    Transport &get(Kind kind) noexcept {
        switch (kind) {
            case Kind::Wifi:
                return wifi_udp;

            case Kind::Espnow:
            default:
                return espnow;
        }
    }

    EspnowTransport espnow{};
    WifiUdpTransport wifi_udp{this->config().wifi_udp};
};

}// namespace botix::transport
