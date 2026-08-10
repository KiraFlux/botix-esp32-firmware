// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/BytesView.hpp>
#include <kf/Option.hpp>
#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Link.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct Link : kf::mixin::NonCopyable {

    void set(protocol::Protocol &new_protocol) noexcept {
        _protocol = kf::someRef(new_protocol);
    }

    void poll(Protocol::PollContext const &context) noexcept {
        if (_protocol.isSome()) {
            _protocol.unwrap().poll(context);
        }
    }

    void receive(Protocol::ReceiveContext const &context) noexcept {
        if (_protocol.isSome()) {
            _protocol.unwrap().receive(context);
        }
    }

private:
    kf::Option<protocol::Protocol &> _protocol{kf::none};
};

}// namespace botix::protocol