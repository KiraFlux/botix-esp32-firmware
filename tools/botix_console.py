#!/usr/bin/env python3
# Copyright (c) 2026 KiraFlux
# SPDX-License-Identifier: GPL-3.0-or-later

"""Send MAVLink commands to a Botix robot over WiFi UDP.

The robot advertises itself over mDNS as ``_botix._udp.local.`` and accepts
MAVLink on the port from ``user.udp.local_port``.  Telemetry and console
replies are sent back to ``user.udp.remote_ip`` / ``user.udp.remote_port``,
so this tool binds that port rather than relying on an ephemeral one.

Modes:
  shell    interactive console over MAVLink SERIAL_CONTROL (default)
  cmd      run a single console command and print the reply
  teleop   drive the robot with WASD via MANUAL_CONTROL
  watch    print inbound telemetry

Examples:
  ./botix_console.py --discover
  ./botix_console.py --host botix.local shell
  ./botix_console.py --host 192.168.1.42 cmd "config list wifi"
  ./botix_console.py --host botix.local teleop
"""

from __future__ import annotations

import argparse
import selectors
import socket
import sys
import termios
import time
import tty
from dataclasses import dataclass
from typing import Iterator

try:
    from pymavlink.dialects.v20 import common as mavlink2
except ImportError:
    sys.exit("pymavlink is required: pip install -r tools/requirements.txt")

# Matches SERIAL_CONTROL_DEV_SHELL in the MAVLink common dialect
SERIAL_CONTROL_DEV_SHELL = 10
SERIAL_CONTROL_FLAG_REPLY = 1
SERIAL_CONTROL_FLAG_RESPOND = 2

# Must match MAVLINK_MSG_SERIAL_CONTROL_FIELD_DATA_LEN
SERIAL_CONTROL_CHUNK = 70

MDNS_SERVICE_TYPE = "_botix._udp.local."

DEFAULT_ROBOT_PORT = 14550
DEFAULT_LOCAL_PORT = 14555

# The ESP32 responder answers a service browse only after a few query rounds;
# measured at roughly five seconds, so anything shorter reports a false negative.
DEFAULT_DISCOVER_TIMEOUT = 8.0


def resolve(host: str) -> str:
    """Resolve a host, falling back to an mDNS browse for a .local name.

    The system resolver needs nss-mdns to answer .local, and it is flaky in
    practice: the same name resolves one minute and fails the next. Browsing
    ourselves keeps --host botix.local working regardless.
    """
    try:
        return socket.gethostbyname(host)
    except socket.gaierror:
        if not host.endswith(".local"):
            raise

    print(f"{host} did not resolve, browsing mDNS instead", file=sys.stderr)

    wanted = host.removesuffix(".local")

    for name, address, _ in discover():
        if name == wanted:
            print(f"{host} -> {address}", file=sys.stderr)
            return address

    sys.exit(f"could not resolve {host}; pass --host <ip> instead")


class _UdpWriter:
    """File-like sink so pymavlink's MAVLink object can emit datagrams."""

    def __init__(self, sock: socket.socket, address: tuple[str, int]) -> None:
        self._sock = sock
        self._address = address

    def write(self, data: bytes) -> None:
        self._sock.sendto(data, self._address)


@dataclass
class Link:
    """A bound UDP socket plus a MAVLink codec aimed at one robot."""

    sock: socket.socket
    mav: mavlink2.MAVLink
    target_system: int

    @classmethod
    def open(
        cls,
        host: str,
        robot_port: int,
        local_port: int,
        source_system: int,
        target_system: int,
    ) -> "Link":
        address = (resolve(host), robot_port)

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", local_port))
        sock.setblocking(False)

        mav = mavlink2.MAVLink(
            _UdpWriter(sock, address),
            srcSystem=source_system,
            srcComponent=mavlink2.MAV_COMP_ID_MISSIONPLANNER,
        )
        mav.robust_parsing = True

        return cls(sock=sock, mav=mav, target_system=target_system)

    def close(self) -> None:
        self.sock.close()

    def receive(self, timeout: float) -> Iterator[object]:
        """Yield every MAVLink message arriving within `timeout` seconds."""
        deadline = time.monotonic() + timeout

        selector = selectors.DefaultSelector()
        selector.register(self.sock, selectors.EVENT_READ)

        try:
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return

                if not selector.select(remaining):
                    return

                try:
                    data, _ = self.sock.recvfrom(2048)
                except BlockingIOError:
                    continue

                for message in self.mav.parse_buffer(data) or []:
                    yield message
        finally:
            selector.close()

    def send_console(self, text: str) -> None:
        """Deliver console input to the robot, split across SERIAL_CONTROL messages."""
        payload = text.encode("utf-8", errors="replace")

        for start in range(0, max(len(payload), 1), SERIAL_CONTROL_CHUNK):
            chunk = payload[start : start + SERIAL_CONTROL_CHUNK]
            padded = chunk + bytes(SERIAL_CONTROL_CHUNK - len(chunk))

            # target_system / target_component are MAVLink v2 extension fields and
            # pymavlink's generated sender omits them. The robot does not filter on
            # them, so the base-field form is sufficient.
            self.mav.serial_control_send(
                SERIAL_CONTROL_DEV_SHELL,
                SERIAL_CONTROL_FLAG_RESPOND,
                0,  # timeout
                0,  # baudrate: no change
                len(chunk),
                padded,
            )

    def send_manual_control(self, x: int, y: int, z: int, r: int) -> None:
        """Send one control frame.

        The robot's tank mixer reads z as drive and r as turn; x and y go to the
        arm and claw servos. Putting throttle in x moves the arm, not the wheels.
        """
        self.mav.manual_control_send(self.target_system, x, y, z, r, 0)

    def send_drive(self, drive: int, turn: int) -> None:
        self.send_manual_control(0, 0, drive, turn)

    def send_stop(self) -> None:
        self.send_manual_control(0, 0, 0, 0)


def console_text(message: object) -> str | None:
    """Extract shell output from a SERIAL_CONTROL reply, if that is what this is."""
    if message.get_type() != "SERIAL_CONTROL":
        return None

    if message.device != SERIAL_CONTROL_DEV_SHELL:
        return None

    return bytes(message.data[: message.count]).decode("utf-8", errors="replace")


def drain(link: Link, timeout: float) -> str:
    """Collect console output arriving within `timeout`."""
    collected = []

    for message in link.receive(timeout):
        text = console_text(message)
        if text:
            collected.append(text)

    return "".join(collected)


def discover(timeout: float = DEFAULT_DISCOVER_TIMEOUT) -> list[tuple[str, str, int]]:
    """Browse mDNS for Botix robots. Returns (name, address, port) triples."""
    try:
        from zeroconf import ServiceBrowser, ServiceListener, Zeroconf
    except ImportError:
        sys.exit("zeroconf is required for --discover: pip install -r tools/requirements.txt")

    found: list[tuple[str, str, int]] = []

    class _Listener(ServiceListener):
        def add_service(self, zc, type_, name):
            info = zc.get_service_info(type_, name)
            if info and info.addresses:
                found.append(
                    (
                        name.removesuffix("." + MDNS_SERVICE_TYPE),
                        socket.inet_ntoa(info.addresses[0]),
                        info.port,
                    )
                )

        def update_service(self, zc, type_, name):
            pass

        def remove_service(self, zc, type_, name):
            pass

    print(f"browsing {MDNS_SERVICE_TYPE} for {timeout:.0f}s...", file=sys.stderr)

    zeroconf = Zeroconf()
    try:
        ServiceBrowser(zeroconf, MDNS_SERVICE_TYPE, _Listener())

        # Stop as soon as something answers rather than always waiting it out
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and not found:
            time.sleep(0.2)
    finally:
        zeroconf.close()

    return found


def run_shell(link: Link) -> int:
    print("Botix console. Type 'help' for commands, Ctrl-D to exit.")

    # Surface anything the robot volunteers before the first prompt
    banner = drain(link, 0.3)
    if banner:
        print(banner, end="")

    while True:
        try:
            line = input("botix> ")
        except (EOFError, KeyboardInterrupt):
            print()
            return 0

        if not line.strip():
            continue

        link.send_console(line + "\n")

        reply = drain(link, 0.5)
        if reply:
            print(reply, end="")
        else:
            print("(no reply)", file=sys.stderr)


def run_cmd(link: Link, command: str, timeout: float) -> int:
    link.send_console(command + "\n")

    reply = drain(link, timeout)
    if not reply:
        print("(no reply)", file=sys.stderr)
        return 1

    print(reply, end="")
    return 0


def run_teleop(link: Link, speed: int, rate_hz: float, hold_s: float = 1.0) -> int:
    print(
        "Teleop: W/S drive, A/D turn, space stop, Q quit.\n"
        f"A command latches and is resent at {rate_hz:.0f} Hz, so holding a key is not\n"
        f"required, but it is cancelled after {hold_s:.1f}s without any keypress.\n"
        "The robot also zeroes its motors if control input goes stale."
    )

    period = 1.0 / rate_hz

    settings = termios.tcgetattr(sys.stdin)
    selector = selectors.DefaultSelector()
    selector.register(sys.stdin, selectors.EVENT_READ)

    drive = 0
    turn = 0
    last_key = time.monotonic()

    try:
        tty.setraw(sys.stdin.fileno())

        while True:
            if selector.select(period):
                key = sys.stdin.read(1).lower()
                last_key = time.monotonic()

                if key in ("q", "\x03"):  # q or Ctrl-C
                    link.send_stop()
                    return 0
                elif key == "w":
                    drive, turn = speed, 0
                elif key == "s":
                    drive, turn = -speed, 0
                elif key == "a":
                    drive, turn = 0, -speed
                elif key == "d":
                    drive, turn = 0, speed
                elif key == " ":
                    drive, turn = 0, 0

            # Dead man: a single tap must not drive the robot indefinitely
            elif (drive or turn) and time.monotonic() - last_key > hold_s:
                drive, turn = 0, 0

            link.send_drive(drive, turn)
    finally:
        selector.close()
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)

        # Leave the robot stopped even if this exits on an exception
        try:
            link.send_stop()
        except OSError:
            pass


def run_watch(link: Link) -> int:
    print("Watching inbound MAVLink. Ctrl-C to stop.")

    try:
        while True:
            for message in link.receive(1.0):
                text = console_text(message)
                if text:
                    sys.stdout.write(text)
                    sys.stdout.flush()
                else:
                    print(message)
    except KeyboardInterrupt:
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--host", help="robot hostname or IP, for example botix.local")
    parser.add_argument("--port", type=int, default=DEFAULT_ROBOT_PORT, help="robot UDP port (user.udp.local_port)")
    parser.add_argument("--local-port", type=int, default=DEFAULT_LOCAL_PORT, help="local UDP port (user.udp.remote_port)")
    parser.add_argument("--source-system", type=int, default=255, help="MAVLink system id of this tool")
    parser.add_argument("--target-system", type=int, default=1, help="MAVLink system id of the robot (device.mavlink.sysid_self)")
    parser.add_argument("--timeout", type=float, default=2.0, help="reply timeout for cmd mode")
    parser.add_argument("--discover", action="store_true", help="browse mDNS for robots and exit")
    parser.add_argument(
        "--discover-timeout",
        type=float,
        default=DEFAULT_DISCOVER_TIMEOUT,
        help="how long to browse before giving up",
    )
    subparsers = parser.add_subparsers(dest="mode")

    subparsers.add_parser("shell")

    cmd_parser = subparsers.add_parser("cmd")
    cmd_parser.add_argument("command", help="console command to run")

    teleop_parser = subparsers.add_parser("teleop")
    teleop_parser.add_argument("--speed", type=int, default=500, help="drive magnitude, 0..1000")
    teleop_parser.add_argument("--rate", type=float, default=20.0, help="command rate in Hz")
    teleop_parser.add_argument(
        "--hold",
        type=float,
        default=1.0,
        help="seconds a command survives without a keypress before it is cancelled",
    )

    subparsers.add_parser("watch")

    args = parser.parse_args()

    if args.discover:
        robots = discover(args.discover_timeout)
        if not robots:
            print("no robots found; the robot answers a browse only after a few seconds, try --discover-timeout 15")
            return 1

        for name, address, port in robots:
            print(f"{name}\t{address}:{port}")
        return 0

    if not args.host:
        parser.error("--host is required (or use --discover)")

    link = Link.open(
        host=args.host,
        robot_port=args.port,
        local_port=args.local_port,
        source_system=args.source_system,
        target_system=args.target_system,
    )

    try:
        mode = args.mode or "shell"

        if mode == "shell":
            return run_shell(link)
        if mode == "cmd":
            return run_cmd(link, args.command, args.timeout)
        if mode == "teleop":
            return run_teleop(link, args.speed, rate_hz=args.rate, hold_s=args.hold)
        if mode == "watch":
            return run_watch(link)

        parser.error(f"unknown mode {mode}")
    finally:
        link.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
