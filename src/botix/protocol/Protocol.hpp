// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Slice.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/transport/TransportLink.hpp"

namespace botix::protocol {

struct Protocol : kf::mixin::NonCopyable {

    // TODO: add telemetry here
    virtual void poll(kf::units::Milliseconds now, transport::TransportLink &transport_link) noexcept = 0;

    virtual void receive(kf::Slice<kf::u8 const> buffer) noexcept = 0;
};

}// namespace botix