// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/NonCopyable.hpp>

#include "botix/protocol/MavlinkProtocol.hpp"
#include "botix/protocol/Protocol.hpp"
#include "botix/protocol/RawProtocol.hpp"

namespace botix::protocol {

struct ProtocolRegistry : kf::mixin::NonCopyable {

    enum class Kind : kf::u8 {
        Raw = 0x00,
        Mavlink = 0x01,
    };

    Protocol &get(Kind kind) noexcept {
        switch (kind) {
            case Kind::Raw: return raw();
            case Kind::Mavlink: return mavlink();
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
    MavlinkProtocol _mavlink_protocol{};
};

}// namespace botix::protocol