// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/BytesView.hpp>
#include <kf/Function.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>

#include <kf/mixin/Callbacked.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/transport/Address.hpp"

namespace botix::internal {

struct TransportOnReceiveCallback : private kf::mixin::Callbacked<void(kf::BytesView)> {
    void onReceive(auto &&f) noexcept {
        this->callback(std::forward<decltype(f)>(f));
    }

protected:
    void invokeReceiveCallback(kf::BytesView buffer) noexcept {
        this->invoke(buffer);
    }
};

struct TransportOnReceiveForeignCallback : private kf::mixin::Callbacked<void(transport::Address const &, kf::BytesView)> {
    void onReceiveForeign(auto &&f) noexcept {
        this->callback(std::forward<decltype(f)>(f));
    }

protected:
    void invokeReceiveForeignCallback(transport::Address const &address, kf::BytesView buffer) noexcept {
        this->invoke(address, buffer);
    }
};

}// namespace botix::internal

namespace botix::transport {

struct Transport :

    kf::mixin::NonCopyable,
    internal::TransportOnReceiveCallback,
    internal::TransportOnReceiveForeignCallback

{
    explicit constexpr Transport(Kind kind) noexcept :
        _kind{kind} {}

    // dynamic interface

    [[nodiscard]] virtual bool send(kf::BytesView buffer) noexcept = 0;

protected:
    [[nodiscard]] virtual bool doConnect(Address const &address) noexcept = 0;

    virtual void doDisconnect() noexcept = 0;

public:
    // properties

    [[nodiscard]] constexpr bool connected() const noexcept {
        return _active_connection_address.isSome();
    }

    [[nodiscard]] constexpr auto activeAddress() const noexcept -> kf::Option<Address const &> {
        return _active_connection_address.isNone() ? kf::none : kf::someRef(_active_connection_address.unwrap());
    }

    // control

    [[nodiscard]] bool connect(Address const &address) noexcept {
        if (connected()) {
            if (_active_connection_address.unwrap() == address) {
                return true;// already on this peer
            }

            disconnect();
        }

        if (_kind != address.kind()) {
            return false;// transport kind mismatch
        }

        if (not doConnect(address)) {
            return false;// connection failed
        }

        _active_connection_address = kf::some(address);

        return true;
    }

    void disconnect() noexcept {
        doDisconnect();
        _active_connection_address.reset();
    }

private:
    kf::Option<Address> _active_connection_address{kf::none};
    Kind const _kind;
};

}// namespace botix::transport