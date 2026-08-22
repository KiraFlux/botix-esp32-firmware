// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "botix/protocol/Kind.hpp"
#include "botix/transport/Kind.hpp"

#include "botix/config/Config.hpp"

namespace botix::config {

struct UserConfig : Config<UserConfig, 0> {

    transport::Kind boot_transport_kind{transport::Kind::Espnow};
    protocol::Kind boot_protocol_kind{protocol::Kind::Mavlink};
};

}// namespace botix::config