// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/BytesView.hpp>
#include <kf/Timer.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>

#include "botix/IncomingTelemetry.hpp"
#include "botix/transport/Address.hpp"
#include "botix/transport/Link.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct RawProtocol :

    Protocol,
    kf::mixin::Callbacked<void(transport::Address const &address, kf::BytesView)>

{
    
    void poll(kf::units::Milliseconds now, transport::Link &transport_link) noexcept override {
        // TODO: bulk send telemetry here

        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);

            (void) transport_link.writeByte(0xAA);
        }
    }

    void receive(ReceiveContext const &context) noexcept override {
        switch (context.transport.buffer.length()) {
            case sizeof(IncomingTelemetry::ControlInput):
                context.telemetry.control_input.update(
                    *reinterpret_cast<IncomingTelemetry::ControlInput const *>(context.transport.buffer.data()),
                    context.timestamp);
                return;

            default:
                this->invoke(context.transport.address, context.transport.buffer);
                return;
        }
    }

private:
    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 2000};

    kf::Timer _heartbeat_timer{heartbeat_timer_config};
};

}// namespace botix::protocol