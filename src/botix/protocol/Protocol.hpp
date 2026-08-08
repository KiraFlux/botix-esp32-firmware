// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/BytesView.hpp>
#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Link.hpp"
#include "botix/transport/Receiver.hpp"

namespace botix::protocol {

struct Protocol : kf::mixin::NonCopyable {

    struct ReceiveContext {
        transport::Receiver::ReceiveContext transport;
        IncomingTelemetry &telemetry;
        kf::units::Milliseconds timestamp;
    };

    // TODO: add telemetry here
    virtual void poll(kf::units::Milliseconds now, transport::Link &transport_link) noexcept = 0;

    virtual void receive(ReceiveContext const &context) noexcept = 0;
};

}// namespace botix::protocol