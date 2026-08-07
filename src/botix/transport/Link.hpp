// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>

#include <kf/BytesView.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/concepts.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/BinaryWritable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

#include "botix/transport/Address.hpp"
#include "botix/transport/Transport.hpp"

namespace botix::transport {

struct Link final :

    kf::mixin::NonCopyable,
    kf::mixin::TimedPollable<Link>,
    kf::mixin::BinaryWritable<Link, bool>

{
    using Self = Link;

    void set(Transport &new_transport) noexcept {
        if (connected()) {
            disconnect();
        }

        _transport = kf::someRef(new_transport);
    }

    // properties

    [[nodiscard]] constexpr bool connected() const noexcept {
        return _transport.isSome() and _transport.unwrap().connected();
    }

    [[nodiscard]] constexpr auto activeAddress() const noexcept -> kf::Option<Address const &> {
        return _transport.isSome() ? kf::none : _transport.unwrap().activeAddress();
    }

    // control

    [[nodiscard]] bool connect(Address const &address) noexcept {
        if (_transport.isNone()) {
            return false;
        }

        return _transport.unwrap().connect(address);
    }

    void disconnect() noexcept {
        if (_transport.isNone()) {
            return;
        }

        _transport.unwrap().disconnect();
    }

private:
    kf::Option<Transport &> _transport{kf::none};

    KF_IMPL_TIMED_POLLABLE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        if (_transport.isNone()) {
            return;
        }

        _transport.unwrap().poll(now);
    }

    KF_IMPL_BINARY_WRITABLE(Self, bool);

    bool writeBufferImpl(kf::BytesView buffer) noexcept {
        if (_transport.isNone()) {
            return false;
        }

        return _transport.unwrap().send(buffer);
    }

    bool writePacketImpl(kf::trivial auto const &packet) noexcept {
        return this->writeBuffer({
            reinterpret_cast<kf::u8 const *>(&packet),
            sizeof(packet),
        });
    }

    bool writeMixedImpl(kf::trivial auto const &header, kf::BytesView buffer) noexcept {
        kf::u8 mixed_buffer[sizeof(header) + buffer.length()];

        auto const header_data = reinterpret_cast<kf::u8 const *>(&header);
        std::copy(header_data, header_data + sizeof(header), mixed_buffer);
        std::copy(buffer.begin(), buffer.end(), mixed_buffer + sizeof(header));

        return this->writeBuffer(mixed_buffer);
    }
};

}// namespace botix::transport