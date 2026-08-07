// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Option.hpp>
#include <kf/Slice.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/transport/TransportLink.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct ProtocolLink : kf::mixin::NonCopyable {

    void protocol(protocol::Protocol &new_protocol) noexcept {
        _protocol = kf::someRef(new_protocol);
    }

    // TODO: add telemetry here
    void poll(kf::units::Milliseconds now, transport::TransportLink &transport_link) noexcept {
        if (_protocol.isSome()) {
            _protocol.unwrap().poll(now, transport_link);
        }
    }

    void receive(kf::Slice<kf::u8 const> buffer) noexcept {
        if (_protocol.isSome()) {
            _protocol.unwrap().receive(buffer);
        }
    }

private:
    kf::Option<protocol::Protocol &> _protocol{kf::none};
};

}// namespace botix::protocol