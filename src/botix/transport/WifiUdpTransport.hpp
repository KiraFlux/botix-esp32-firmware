// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

#include <kf/BytesView.hpp>
#include <kf/Logger.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/transport/Address.hpp"
#include "botix/transport/IpEndpoint.hpp"
#include "botix/transport/Ipv4.hpp"
#include "botix/transport/Receiver.hpp"
#include "botix/transport/Transport.hpp"

namespace botix::internal {

/// @brief Configuration for the WiFi UDP transport
struct WifiUdpTransportConfig : kf::mixin::Resettable<WifiUdpTransportConfig> {

    /// @brief Local port the robot listens on
    kf::u16 local_port;

    /// @brief Remote endpoint packets are sent to
    transport::IpEndpoint remote;

private:
    KF_IMPL_RESETTABLE(WifiUdpTransportConfig);
    constexpr void resetImpl() noexcept {
        local_port = 14550;// MAVLink default
        remote.address.value = 0;
        remote.port = 14555;
    }
};

}// namespace botix::internal

namespace botix::transport {

/// @brief Connectionless UDP transport over a WiFi station link
/// @note The remote endpoint is taken from configuration rather than learned
///       from inbound traffic, so `connect()` never performs any handshake.
struct WifiUdpTransport final :

    Transport,
    kf::mixin::Configured<internal::WifiUdpTransportConfig>,
    kf::mixin::Initable<WifiUdpTransport, bool()>

{
    using Self = WifiUdpTransport;

    using Config = internal::WifiUdpTransportConfig;

    /// @note Not constexpr: the underlying `WiFiUDP` has a runtime constructor
    explicit WifiUdpTransport(Config const &config) noexcept :
        Transport{Kind::Wifi},
        kf::mixin::Configured<Config>{config} {}

    /// @brief Endpoint this transport should connect to, per configuration
    [[nodiscard]] constexpr Address configuredAddress() const noexcept {
        return Address::createForWifi(this->config().remote);
    }

    /// @brief Whether the socket is bound and the station has an IP
    [[nodiscard]] bool ready() const noexcept {
        return _listening and WiFi.status() == WL_CONNECTED;
    }

    void poll(kf::units::Milliseconds now) noexcept override {
        (void) now;

        if (not _listening) {
            // The socket cannot bind before the station holds an address
            if (WiFi.status() == WL_CONNECTED) {
                (void) startListening();
            }
            return;
        }

        // A dropped station link invalidates the bound socket
        if (WiFi.status() != WL_CONNECTED) {
            _udp.stop();
            _listening = false;
            return;
        }

        drain();
    }

    bool send(kf::BytesView buffer) noexcept override {
        if (not _listening or buffer.empty()) {
            return false;
        }

        auto const maybe_address = activeAddress();
        if (maybe_address.isNone()) {
            return false;
        }

        auto const &remote = maybe_address.unwrap().endpoint();
        if (remote.empty()) {
            return false;
        }

        IPAddress const ip{
            remote.address.octet(0),
            remote.address.octet(1),
            remote.address.octet(2),
            remote.address.octet(3),
        };

        if (_udp.beginPacket(ip, remote.port) != 1) {
            return false;
        }

        _udp.write(buffer.data(), buffer.length());

        return _udp.endPacket() == 1;
    }

protected:
    bool doConnect(Address const &address) noexcept override {
        // UDP carries no session: a non-empty endpoint is all that is needed
        return not address.endpoint().empty();
    }

    void doDisconnect() noexcept override {
        // Nothing to tear down, the socket stays bound for inbound traffic
    }

private:
    inline static char logger_buffer[64]{};

    inline static kf::Logger logger{"WifiUdpTransport", {logger_buffer}};

    /// @brief Largest datagram accepted in one read
    static constexpr kf::usize _receive_buffer_length{512};

    WiFiUDP _udp{};
    kf::u8 _receive_buffer[_receive_buffer_length]{};
    bool _listening{false};

    [[nodiscard]] bool startListening() noexcept {
        if (_udp.begin(this->config().local_port) != 1) {
            logger.error("UDP bind failed on port {}", this->config().local_port);
            return false;
        }

        _listening = true;
        logger.info("Listening on port {}", this->config().local_port);
        return true;
    }

    void drain() noexcept {
        auto maybe_receiver = receiver();
        if (maybe_receiver.isNone()) {
            return;
        }

        auto &target = maybe_receiver.unwrap();

        while (true) {
            auto const packet_length = _udp.parsePacket();
            if (packet_length <= 0) {
                return;
            }

            auto const read = _udp.read(_receive_buffer, _receive_buffer_length);
            if (read <= 0) {
                continue;
            }

            auto const remote_ip = _udp.remoteIP();

            IpEndpoint source{};
            source.address = Ipv4::fromOctets(remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3]);
            source.port = static_cast<kf::u16>(_udp.remotePort());

            if (source.empty()) {
                continue;
            }

            Receiver::ReceiveContext const context{
                .address = Address::createForWifi(source),
                .buffer = {_receive_buffer, static_cast<kf::usize>(read)},
            };

            auto const maybe_active = activeAddress();

            if (maybe_active.isSome() and maybe_active.unwrap().endpoint() == source) {
                target.invokeReceiveCallback(context);
            } else {
                target.invokeReceiveForeignCallback(context);
            }
        }
    }

    KF_IMPL_INITABLE(Self, bool());
    bool initImpl() noexcept {
        // Binding is deferred to poll(): the station may not hold an IP yet
        _listening = false;
        return true;
    }
};

}// namespace botix::transport
