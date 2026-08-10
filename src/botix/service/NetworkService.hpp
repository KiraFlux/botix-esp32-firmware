// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ESPmDNS.h>
#include <WiFi.h>

#include <kf/Logger.hpp>
#include <kf/StringView.hpp>
#include <kf/Timer.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/service/Service.hpp"

namespace botix::internal {

/// @brief Maximum stored length of a text network field, excluding the terminator
enum : kf::usize {
    network_ssid_capacity = 32,
    network_password_capacity = 64,
    network_hostname_capacity = 32,
};

/// @brief Configuration of the WiFi station link and mDNS advertisement
struct NetworkServiceConfig : kf::mixin::Resettable<NetworkServiceConfig> {

    /// @brief Access point to join
    char ssid[network_ssid_capacity + 1];

    /// @brief WPA passphrase, empty for an open network
    char password[network_password_capacity + 1];

    /// @brief mDNS name, reachable as `<hostname>.local`
    char hostname[network_hostname_capacity + 1];

    /// @brief Delay before a stalled association attempt is retried
    kf::Timer::Config retry_timer;

    /// @brief Whether the station should be brought up at all
    bool enabled;

private:
    KF_IMPL_RESETTABLE(NetworkServiceConfig);
    constexpr void resetImpl() noexcept {
        ssid[0] = '\0';
        password[0] = '\0';

        hostname[0] = 'b';
        hostname[1] = 'o';
        hostname[2] = 't';
        hostname[3] = 'i';
        hostname[4] = 'x';
        hostname[5] = '\0';

        retry_timer.value = 10'000;
        enabled = false;
    }
};

}// namespace botix::internal

namespace botix::service {

/// @brief Brings up the WiFi station link and publishes the robot over mDNS
struct NetworkService final :

    Service<NetworkService>,
    kf::mixin::Configured<internal::NetworkServiceConfig>

{
    using Self = NetworkService;

    using Config = internal::NetworkServiceConfig;

    enum class State : kf::u8 {
        /// @brief Station disabled by configuration
        Disabled,
        /// @brief Association in progress
        Connecting,
        /// @brief Associated, address acquired, mDNS published
        Connected,
    };

    struct Dependencies {
        Config const &config;
        /// @brief Port advertised over mDNS, owned by the UDP transport config
        kf::u16 const &service_port;
    };

    explicit constexpr NetworkService(Dependencies deps) noexcept :
        kf::mixin::Configured<Config>{deps.config},
        _service_port{deps.service_port} {}

    [[nodiscard]] constexpr State state() const noexcept {
        return _state;
    }

    [[nodiscard]] static kf::StringView stateName(State state) noexcept {
        switch (state) {
            case State::Connecting: return "connecting";
            case State::Connected: return "connected";
            case State::Disabled:
            default: return "disabled";
        }
    }

    /// @brief Local IPv4 address in host byte order, zero when not associated
    [[nodiscard]] kf::u32 localAddress() const noexcept {
        if (_state != State::Connected) {
            return 0;
        }

        auto const ip = WiFi.localIP();

        return (static_cast<kf::u32>(ip[0]) << 24) |
               (static_cast<kf::u32>(ip[1]) << 16) |
               (static_cast<kf::u32>(ip[2]) << 8) |
               static_cast<kf::u32>(ip[3]);
    }

    /// @brief Drop the current association so the next poll re-reads configuration
    void reconnect() noexcept {
        teardown();
        _state = State::Disabled;
    }

private:
    inline static char logger_buffer[96]{};

    inline static kf::Logger logger{"NetworkService", {logger_buffer}};

    kf::u16 const &_service_port;
    kf::Timer _retry_timer{this->config().retry_timer};
    State _state{State::Disabled};
    bool _mdns_started{false};

    [[nodiscard]] static kf::StringView asView(char const *field, kf::usize capacity) noexcept {
        kf::usize length = 0;
        while (length < capacity and field[length] != '\0') {
            length += 1;
        }
        return {field, length};
    }

    void teardown() noexcept {
        if (_mdns_started) {
            MDNS.end();
            _mdns_started = false;
        }

        (void) WiFi.disconnect(false, false);
    }

    void beginAssociation(kf::units::Milliseconds now) noexcept {
        auto const &c = this->config();

        if (c.ssid[0] == '\0') {
            // Nothing to join; stay disabled until an SSID is configured
            return;
        }

        (void) WiFi.setHostname(c.hostname);

        logger.info("Joining '{}'", asView(c.ssid, internal::network_ssid_capacity));

        (void) WiFi.begin(c.ssid, c.password[0] == '\0' ? nullptr : c.password);

        _retry_timer.start(now);

        _state = State::Connecting;
    }

    void publishMdns() noexcept {
        auto const &c = this->config();

        if (not MDNS.begin(c.hostname)) {
            logger.error("mDNS begin failed");
            return;
        }

        MDNS.addService("botix", "udp", _service_port);
        _mdns_started = true;

        logger.info("Published as '{}.local' on port {}", asView(c.hostname, internal::network_hostname_capacity), _service_port);
    }

    BOTIX_IMPL_SERVICE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        if (not this->config().enabled) {
            if (_state != State::Disabled) {
                logger.info("Disabled by config");
                teardown();
                _state = State::Disabled;
            }
            return;
        }

        switch (_state) {
            case State::Disabled: {
                beginAssociation(now);
                return;
            }

            case State::Connecting: {
                if (WiFi.status() == WL_CONNECTED) {
                    auto const ip = WiFi.localIP();
                    logger.info("Associated, address {}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3]);

                    publishMdns();
                    _state = State::Connected;
                    return;
                }

                if (_retry_timer.expired(now)) {
                    logger.warn("Association timed out, retrying");
                    teardown();
                    beginAssociation(now);
                }
                return;
            }

            case State::Connected: {
                if (WiFi.status() != WL_CONNECTED) {
                    logger.warn("Link lost");
                    teardown();
                    beginAssociation(now);
                }
                return;
            }
        }
    }
};

}// namespace botix::service
