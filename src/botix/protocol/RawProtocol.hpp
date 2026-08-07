// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/BytesView.hpp>
#include <kf/Timer.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Callbacked.hpp>

#include "botix/transport/Link.hpp"

#include "botix/protocol/Protocol.hpp"

namespace botix::protocol {

struct RawProtocol : Protocol, kf::mixin::Callbacked<void(kf::BytesView)> {

    void poll(kf::units::Milliseconds now, transport::Link &transport_link) noexcept override {
        // TODO: bulk send telemetry here

        if (_heartbeat_timer.expired(now)) {
            _heartbeat_timer.start(now);

            (void) transport_link.writeByte(0xAA);
        }
    }

    void receive(kf::BytesView buffer) noexcept override {
        this->invoke(buffer);
    }

private:
    static constexpr kf::Timer::Config heartbeat_timer_config{.value = 2000};

    kf::Timer _heartbeat_timer{heartbeat_timer_config};
};

}// namespace botix::protocol