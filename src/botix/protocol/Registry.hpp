// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/protocol/Kind.hpp"
#include "botix/protocol/MavlinkProtocol.hpp"
#include "botix/protocol/Protocol.hpp"
#include "botix/protocol/RawProtocol.hpp"

namespace botix::internal {

struct ProtocolRegistryConfig : kf::mixin::Resettable<ProtocolRegistryConfig> {

    protocol::MavlinkProtocol::Config mavlink;

private:
    KF_IMPL_RESETTABLE(ProtocolRegistryConfig);
    constexpr void resetImpl() noexcept {
        mavlink.reset();
    }
};

}// namespace botix::internal

namespace botix::protocol {

struct Registry :

    kf::mixin::NonCopyable,
    kf::mixin::Configured<internal::ProtocolRegistryConfig>

{

    using Config = internal::ProtocolRegistryConfig;

    using kf::mixin::Configured<Config>::Configured;

    Protocol &get(Kind kind) noexcept {
        switch (kind) {
            case Kind::Mavlink:
                return mavlink();

            case Kind::Raw:
            default:
                return raw();
        }
    }

    RawProtocol &raw() noexcept {
        return _raw_protocol;
    }

    MavlinkProtocol &mavlink() noexcept {
        return _mavlink_protocol;
    }

private:
    RawProtocol _raw_protocol{};
    MavlinkProtocol _mavlink_protocol{this->config().mavlink};
};

}// namespace botix::protocol