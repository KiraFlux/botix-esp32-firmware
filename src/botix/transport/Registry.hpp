// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/NonCopyable.hpp>

#include "botix/transport/EspnowTransport.hpp"
#include "botix/transport/Kind.hpp"
#include "botix/transport/Transport.hpp"

namespace botix::transport {

struct Registry : kf::mixin::NonCopyable {

    Transport &get(Kind kind) noexcept {
        (void) kind;
        return espnow();
    }

    EspnowTransport &espnow() noexcept {
        return _espnow_transport;
    }

private:
    EspnowTransport _espnow_transport{};
};

}// namespace botix::transport