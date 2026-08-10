// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "botix/protocol/Kind.hpp"
#include "botix/service/NetworkService.hpp"
#include "botix/transport/Kind.hpp"
#include "botix/transport/Registry.hpp"

#include "botix/config/Config.hpp"

namespace botix::config {

/// @brief Deployment-scoped settings: what to start with and how to reach the network
struct UserConfig : Config<UserConfig, 1> {

    transport::Kind init_transport_kind;
    protocol::Kind init_protocol_kind;

    service::NetworkService::Config network;

    transport::Registry::Config transport_registry;

private:
    KF_IMPL_RESETTABLE(UserConfig);
    constexpr void resetImpl() noexcept {
        version = UserConfig::latest_version;
        init_transport_kind = transport::Kind::Espnow;
        init_protocol_kind = protocol::Kind::Mavlink;
        network.reset();
        transport_registry.reset();
    }
};

}// namespace botix::config
