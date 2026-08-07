// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Slice.hpp>
#include <kf/Timer.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>

#include "botix/transport/TransportLink.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct RawProtocol : Protocol, kf::mixin::Callbacked<void(kf::Slice<kf::u8 const>)> {

    void poll(kf::units::Milliseconds now, transport::TransportLink &transport_link) noexcept override {
        // TODO: bulk send telemetry here

        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);

            (void) transport_link.sendPacket(0xAA);
        }
    }

    void receive(kf::Slice<kf::u8 const> buffer) noexcept override {
        this->invoke(buffer);
    }

private:
    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 2000};

    kf::Timer _heartbeat_timer{heartbeat_timer_config};
};

}// namespace botix::protocol