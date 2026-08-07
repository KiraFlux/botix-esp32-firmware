// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/BytesView.hpp>

#include <kf/mixin/Callbacked.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/transport/Address.hpp"

namespace botix::internal {

using OnReceiveBase = kf::mixin::Callbacked<void(transport::Address const &, kf::BytesView)>;

struct OnReceiveCallbacked : private OnReceiveBase {
    void onReceive(auto &&f) noexcept {
        this->callback(std::forward<decltype(f)>(f));
    }

    void invokeReceiveCallback(transport::Address const &address, kf::BytesView buffer) noexcept {
        this->invoke(address, buffer);
    }
};

struct OnReceiveForeignCallbacked : private kf::mixin::Callbacked<void(transport::Address const &, kf::BytesView)> {
    void onReceiveForeign(auto &&f) noexcept {
        this->callback(std::forward<decltype(f)>(f));
    }

    void invokeReceiveForeignCallback(transport::Address const &address, kf::BytesView buffer) noexcept {
        this->invoke(address, buffer);
    }
};

}// namespace botix::internal

namespace botix::transport {

struct Receiver final :

    kf::mixin::NonCopyable,
    internal::OnReceiveCallbacked,
    internal::OnReceiveForeignCallbacked

{
    constexpr Receiver() noexcept = default;
};

}// namespace botix::transport