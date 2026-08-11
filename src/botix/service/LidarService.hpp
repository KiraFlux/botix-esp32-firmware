// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <HardwareSerial.h>
#include <WiFiUdp.h>

#include <kf/Logger.hpp>
#include <kf/primitives.hpp>
#include <kf/units.hpp>

#include <kf/mixin/Configured.hpp>
#include <kf/mixin/Initable.hpp>
#include <kf/mixin/Resettable.hpp>

#include "botix/transport/IpEndpoint.hpp"

#include "botix/service/Service.hpp"

namespace botix::internal {

struct LidarServiceConfig : kf::mixin::Resettable<LidarServiceConfig> {

    /// @brief UART peripheral index; 2 puts RX on GPIO16 on a WROOM-32 devkit
    /// @note 0 is the console and the programming line. Pointing the lidar at it
    ///       fights the USB bridge for the same wire and feeds binary at the
    ///       command parser, so it is rejected at init.
    kf::u8 uart_num;

    kf::u32 baudrate;

    /// @brief Destination port on the configured remote host
    kf::u16 remote_port;

    /// @brief Receive buffer; the default 256 bytes overflows between polls
    /// @note At 230400 baud a 10 ms loop accumulates ~230 bytes, leaving no margin
    kf::u16 rx_buffer_length;

    bool enabled;

    /// @brief GPIO the sensor's data line is wired to
    /// @note Must be given explicitly. The esp32dev variant defines no RX2, so
    ///       HardwareSerial's default-pin branch for UART2 is preprocessed away
    ///       and the peripheral comes up with no pin attached at all: it runs,
    ///       reads nothing, and looks exactly like a dead sensor.
    /// @note Declared after `enabled` so it lands in existing trailing padding.
    ///       sizeof is unchanged, so a stored blob still loads and nobody has to
    ///       retype WiFi credentials. Padding carries no guarantee of content,
    ///       so an implausible value falls back to the default rather than
    ///       binding some unrelated pin.
    kf::u8 rx_pin;

    /// @brief Where the lidar is wired on this chassis
    static constexpr kf::u8 default_rx_pin{16};

    [[nodiscard]] constexpr kf::u8 effectiveRxPin() const noexcept {
        return (rx_pin >= 1 and rx_pin <= 39) ? rx_pin : default_rx_pin;
    }

private:
    KF_IMPL_RESETTABLE(LidarServiceConfig);
    constexpr void resetImpl() noexcept {
        uart_num = 2;
        rx_pin = default_rx_pin;
        baudrate = 230'400;// LD06 / LD19 / D500 family
        remote_port = 14560;
        rx_buffer_length = 2048;
        enabled = false;
    }
};

}// namespace botix::internal

namespace botix::service {

/// @brief Forwards a TX-only UART lidar to the host as UDP datagrams
/// @note Deliberately does not parse. The exact framing varies across the LD06
///       family and its clones, so protocol knowledge lives in the host driver
///       where a mistake costs a re-run rather than a reflash. Each datagram
///       carries a sequence number so the host can spot loss and resynchronise.
struct LidarService final :

    Service<LidarService>,
    kf::mixin::Configured<internal::LidarServiceConfig>,
    kf::mixin::Initable<LidarService, bool()>

{
    using Self = LidarService;
    using Config = internal::LidarServiceConfig;

    /// @brief Datagram header, little-endian, ahead of every chunk of UART bytes
    struct Header {
        kf::u8 magic;  ///< 'L'
        kf::u8 version;///< 1
        kf::u16 sequence;
    } __attribute__((packed));

    static constexpr kf::u8 header_magic{'L'};
    static constexpr kf::u8 header_version{1};

    /// @brief Largest UART payload per datagram, kept under a 1500-byte MTU
    static constexpr kf::usize max_payload{1024};

    struct Dependencies {
        Config const &config;
        transport::IpEndpoint const &remote;
    };

    explicit constexpr LidarService(Dependencies deps) noexcept :
        kf::mixin::Configured<Config>{deps.config}, _remote{deps.remote} {}

    [[nodiscard]] constexpr kf::u32 bytesRead() const noexcept { return _bytes_read; }
    [[nodiscard]] constexpr kf::u32 datagrams() const noexcept { return _datagrams; }
    [[nodiscard]] constexpr kf::u32 bytesForwarded() const noexcept { return _bytes_forwarded; }
    [[nodiscard]] constexpr kf::u32 sendFailures() const noexcept { return _send_failures; }
    [[nodiscard]] constexpr bool running() const noexcept { return _running; }

private:
    kf::Logger _logger{"LidarService"};

    transport::IpEndpoint const &_remote;
    HardwareSerial *_serial{nullptr};
    WiFiUDP _udp{};

    kf::u32 _bytes_read{0}, _datagrams{0}, _bytes_forwarded{0}, _send_failures{0};
    kf::u16 _sequence{0};
    bool _running{false};

    KF_IMPL_INITABLE(Self, bool());
    bool initImpl() noexcept {
        if (not this->config().enabled) {
            _logger.info("Disabled");
            return true;
        }

        if (this->config().uart_num == 0) {
            _logger.error("UART 0 is the console and the programming line; use 2 (RX on GPIO16)");
            return false;
        }

        _serial = new HardwareSerial(this->config().uart_num);

        // Must precede begin() to take effect
        (void) _serial->setRxBufferSize(this->config().rx_buffer_length);
        // Pins passed explicitly; -1 for TX leaves GPIO17 free, and the sensor
        // has no receive line to drive anyway
        _serial->begin(
            this->config().baudrate,
            SERIAL_8N1,
            static_cast<int8_t>(this->config().effectiveRxPin()),
            -1);

        _running = true;
        _logger.info(
            "UART{} rx=GPIO{} at {} baud -> port {}",
            this->config().uart_num,
            this->config().effectiveRxPin(),
            this->config().baudrate,
            this->config().remote_port);
        return true;
    }

    BOTIX_IMPL_SERVICE(Self);
    void pollImpl(kf::units::Milliseconds now) noexcept {
        (void) now;

        if (not _running) {
            return;
        }

        while (auto const available = static_cast<kf::usize>(_serial->available())) {
            kf::u8 payload[max_payload];

            auto const to_read = (available > max_payload) ? max_payload : available;
            auto const read = static_cast<kf::usize>(_serial->readBytes(payload, to_read));

            if (read == 0) {
                return;
            }

            _bytes_read += read;

            // Keep draining even with nowhere to send: the byte counter is the
            // only evidence that the sensor itself is alive, and demanding a
            // configured host first would make a wiring fault and a network
            // fault look identical.
            if (not _remote.empty()) {
                forward({payload, read});
            }

            // A partial fill means the buffer is drained; leave the rest of the
            // budget to the other systems in this iteration
            if (read < max_payload) {
                return;
            }
        }
    }

    void forward(kf::BytesView payload) noexcept {
        Header const header{
            .magic = header_magic,
            .version = header_version,
            .sequence = _sequence,
        };

        IPAddress const destination{
            _remote.address.octet(0),
            _remote.address.octet(1),
            _remote.address.octet(2),
            _remote.address.octet(3),
        };

        if (_udp.beginPacket(destination, this->config().remote_port) != 1) {
            _send_failures += 1;
            return;
        }

        _udp.write(reinterpret_cast<kf::u8 const *>(&header), sizeof(header));
        _udp.write(payload.data(), payload.length());

        if (_udp.endPacket() != 1) {
            _send_failures += 1;
            return;
        }

        _sequence += 1;
        _datagrams += 1;
        _bytes_forwarded += payload.length();
    }
};

}// namespace botix::service
