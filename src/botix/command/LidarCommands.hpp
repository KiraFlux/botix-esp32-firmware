// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>

#include <kf/mixin/NonCopyable.hpp>

#include "botix/service/ConsoleService.hpp"
#include "botix/service/LidarService.hpp"
#include "botix/transport/IpEndpoint.hpp"

namespace botix::command {

/// @brief The `lidar` command: is the sensor talking, and is anything leaving
/// @note The forwarder cannot see whether the host receives, so it reports what
///       it can prove locally: bytes arriving on the UART and datagrams accepted
///       by the stack. Bytes climbing with datagrams flat means the destination
///       is wrong; both flat means nothing is arriving on the wire.
struct LidarCommands : kf::mixin::NonCopyable {

    struct Dependencies {
        service::LidarService &lidar;
        transport::IpEndpoint const &remote;
    };

    explicit constexpr LidarCommands(Dependencies deps) noexcept :
        _lidar{deps.lidar}, _remote{deps.remote} {}

    [[nodiscard]] bool registerIn(service::ConsoleService &console, kf::Arena &arena) noexcept {
        return console.addCommand(arena, "lidar", [this](auto const &call) { report(call); }).isSome();
    }

private:
    using Call = service::ConsoleService::Command::Context;

    service::LidarService &_lidar;
    transport::IpEndpoint const &_remote;

    void report(Call const &call) const noexcept {
        if (not _lidar.config().enabled) {
            call.output.print("disabled; set user.lidar.enabled true and reboot");
            return;
        }

        if (not _lidar.running()) {
            call.output.print("enabled but not running; check the init log");
            return;
        }

        call.output.print("uart               : {}", _lidar.config().uart_num);
        call.output.print("baudrate           : {}", _lidar.config().baudrate);
        call.output.print("destination        : {}:{}", _remote.address, _lidar.config().remote_port);
        call.output.print("bytes_read         : {}", _lidar.bytesRead());
        call.output.print("datagrams          : {}", _lidar.datagrams());
        call.output.print("bytes_forwarded    : {}", _lidar.bytesForwarded());
        call.output.print("send_failures      : {}", _lidar.sendFailures());

        // A handful of bytes is a line transient at init, not a stream. A real
        // sensor at 230400 delivers thousands per second.
        constexpr kf::u32 streaming_threshold{64};

        if (_lidar.bytesRead() < streaming_threshold) {
            call.output.print("note: no stream on the UART. Check power to the lidar, that its");
            call.output.print("      motor actually spins, that TX reaches GPIO16, and that ground");
            call.output.print("      is shared with the board.");
        } else if (_remote.empty()) {
            call.output.print("note: sensor is alive but user.udp.remote_ip is unset, so nothing is sent");
        }
    }
};

}// namespace botix::command
