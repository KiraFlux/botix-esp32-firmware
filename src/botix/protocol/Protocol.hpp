// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/BytesView.hpp>
#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/OutgoingTelemetry.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Link.hpp"
#include "botix/transport/Receiver.hpp"

namespace botix::protocol {

struct Protocol : kf::mixin::NonCopyable {

    struct PollContext {
        transport::Link &transport_link;
        OutgoingTelemetry &outgoing_telemetry;
        kf::units::Milliseconds timestamp;
    };

    virtual void poll(PollContext const &context) noexcept = 0;

    struct ReceiveContext {
        transport::Receiver::ReceiveContext transport;
        IncomingTelemetry &incoming_telemetry;
        kf::units::Milliseconds timestamp;
    };

    virtual void receive(ReceiveContext const &context) noexcept = 0;
};

}// namespace botix::protocol