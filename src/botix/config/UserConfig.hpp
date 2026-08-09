// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "botix/protocol/Kind.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/config/Config.hpp"

namespace botix::config {

struct UserConfig : Config<UserConfig, 0> {

    transport::Kind init_transport_kind;
    protocol::Kind init_protocol_kind;

private:
    KF_IMPL_RESETTABLE(UserConfig);
    constexpr void resetImpl() noexcept {
        version = UserConfig::latest_version;
        init_transport_kind = transport::Kind::Espnow;
        init_protocol_kind = protocol::Kind::Mavlink;
    }
};

}// namespace botix