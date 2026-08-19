# Botix: ESP32 Mobile Robot Firmware

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Modular ESP32 firmware for the [**Botix** open‑source mobile robot platform](https://github.com/KiraFlux/Botix.git)

Exposes the robot's low‑level capabilities to a higher‑level controller (ROS 2 node, PC application, etc.) over **ESP‑NOW** or a **WiFi UDP** link, with a command console reachable over both the serial line and MAVLink.

---

# Software

The firmware is a **PlatformIO** project located in the repository root. All dependencies are declared and managed in [`platformio.ini`](./platformio.ini).

The project uses:
- [KiraFlux/KiraFlux‑Toolkit](https://github.com/KiraFlux/KiraFlux-Toolkit.git)

## Build

A [`makefile`](./makefile) wraps common PlatformIO commands for convenience.

| Make Target    | Shortcut | Action                              | Description                              |
| :------------- | :------- | :---------------------------------- | :--------------------------------------- |
| `make all`     | `make`   | `pio run`                           | Compile the project                      |
| `make clean`   | `make c` | `pio run -t clean`                  | Delete compiled objects                  |
| `make upload`  | `make u` | `pio run -t upload`                 | Compile and upload firmware to the ESP32 |
| `make monitor` | `make m` | `pio device monitor --no-reconnect` | Open serial monitor (no auto-reconnect)  |

## Tests

Host tests cover the platform-independent logic — lexeme parsing and
offset-based configuration access:

```
pio test -e native
```

---

# Console

The firmware exposes a command console. Every channel shares one command
registry, so a command behaves identically over the serial line and over the
air. Two channels are opened at boot:

- **serial** — the UART, at the usual `115200` baud
- **mavlink** — MAVLink `SERIAL_CONTROL` (`device = SERIAL_CONTROL_DEV_SHELL`),
  which makes the same console reachable from a host over WiFi

## Commands

| Command                        | Description                                                |
| :----------------------------- | :--------------------------------------------------------- |
| `help`                         | List commands with their argument signatures                |
| `info`                         | Uptime, free heap, WiFi state and address                   |
| `reboot`                       | Restart the device                                          |
| `telemetry`                    | Last control input with its age, and wheel odometry         |
| `config list [prefix]`         | Show configuration fields, filtered by section or path      |
| `config get <path>`            | Show one field                                              |
| `config set <path> <value>`    | Change one field                                            |
| `config save`                  | Persist both configs to NVS now                             |
| `config reset-device`          | Restore device defaults                                     |
| `config reset-user`            | Restore user defaults                                       |
| `transport status`             | Active transport, connection state and peer                 |
| `transport use <espnow\|wifi>` | Switch the active transport for this session                |
| `transport connect`            | Connect the WiFi transport to the configured remote         |
| `transport disconnect`         | Drop the current connection                                 |
| `protocol <raw\|mavlink>`      | Switch the active protocol for this session                 |

`transport use` and `protocol` affect the running session only. Set
`user.boot.transport` / `user.boot.protocol` to change what is selected at boot.

## Configuration

Every persisted field is addressable by a `section.path` name through the
config registry. `device` holds hardware and protocol tuning, `user` holds
deployment settings. Values are written immediately but only reach NVS on the
sync timer or on `config save`.

Bringing a robot onto a network:

```
config set user.wifi.ssid MyNetwork
config set user.wifi.password MyPassword
config set user.wifi.hostname botix
config set user.wifi.enabled true
config set user.udp.remote_ip 192.168.1.10
config set user.boot.transport wifi
config save
reboot
```

The robot then joins the network, publishes itself over mDNS as
`<hostname>.local` advertising `_botix._udp`, and sends telemetry to
`user.udp.remote_ip:user.udp.remote_port`.

<blockquote>

### Notes and limitations

- **The console is not authenticated.** Anyone able to reach the UDP port can
  read and change configuration, including WiFi credentials. Secret fields are
  masked when displayed, which is a display convenience and not a control. Use
  it on trusted networks only.
- **Values cannot contain spaces.** The console splits input on whitespace, so
  an SSID or password with a space cannot be set this way. Surrounding quotes
  are stripped from text values, which is also the only way to write an empty
  one: `config set user.wifi.password ""`.
- **ESP-NOW and WiFi cannot both be relied on.** Associating with an access
  point changes the radio channel, which breaks an ESP-NOW peer parked on a
  different one. Pick one transport per deployment.
- The remote endpoint is taken from configuration rather than learned from
  inbound traffic, so the host address must be set before telemetry flows.

</blockquote>

---

# Host tooling

[`tools/botix_console.py`](./tools/botix_console.py) drives the robot over
MAVLink from a PC.

```
pip install -r tools/requirements.txt

./tools/botix_console.py --discover                        # find robots via mDNS
./tools/botix_console.py --host botix.local shell          # interactive console
./tools/botix_console.py --host botix.local cmd "info"     # single command
./tools/botix_console.py --host botix.local teleop         # WASD driving
./tools/botix_console.py --host botix.local watch          # print inbound messages
```

The tool binds `user.udp.remote_port` locally, because that is where the robot
sends replies and telemetry; it must match the robot's configuration.

## Teleop

`W`/`S` drive, `A`/`D` turn, space stops, `Q` quits. A command latches and is
resent at 20 Hz, so a key need not be held; it is cancelled after a second
without any keypress, and the robot zeroes its motors whenever control input
goes stale. Raise `--speed` if the chassis does not break static friction:
400 of 1000 was not always enough on a carpet, 700 was.

The tank mixer takes drive from the MAVLink `z` axis and turn from `r`; `x` and
`y` reach the arm and claw servos. Sending throttle on `x` moves the arm and
leaves the wheels still.

## Straightening the drivetrain

Two nominally identical motors differ by a few percent, so the robot curves
under a straight command. `mixer.left_scale` and `mixer.right_scale` trim each
motor in per-mille, 1000 leaving the command untouched. They only attenuate, so
balance by slowing the faster wheel rather than pushing the slower one.

Measure with the odometry rather than by eye. Drive straight for a couple of
seconds, read `telemetry` before and after, and compare the magnitudes — the
two encoders count in opposite directions on a mirrored chassis, so compare
absolute values:

```
config set device.mixer.right_scale 911
config save
```

For the current 67 mm wheels and 950 encoder ticks per wheel revolution, the
calibrated encoder scales are `device.encoder.left_mm_per_tick = -0.221565`
and `device.encoder.right_mm_per_tick = 0.221565`.

On the robot measured here the right wheel ran 9.8% fast over repeated runs,
giving 911.

## Current State

<blockquote>

ESP‑NOW control via an [ESP32 Dual Joystick Controller (DJC)](https://github.com/KiraFlux/ESP32-DJC)
still works and remains the default at boot. The WiFi UDP path is in place
alongside it and is selected with `user.boot.transport`.

</blockquote>

## Planned Features

- WiFi TCP bridge for ROS 2 or other high‑level controllers
- High‑rate odometry telemetry (encoder deltas)
- On‑board voltage monitoring
- Sleep / deep‑sleep for power saving
- Time‑stamped sensor data

---

# Hardware

## Components

| Group      | Component              | Part / Model              |  Qty  | Notes                                     |
| :--------- | :--------------------- | :------------------------ | :---: | :---------------------------------------- |
| **MCU**    | ESP32 DevKit           | ESP32‑WROOM‑32 (V4)       |   1   | Any standard DevKit variant works         |
| **Motor**  | DC geared motor + enc. | JGA25‑370 (with Hall)     |   2   | Motor + dual‑phase quadrature encoder     |
| **Driver** | Motor driver           | DRV8871 module            |   2   | Single H‑bridge, 3.6 A peak               |
| **Power**  | Step‑down converter    | Mini 560 Pro (5 V output) |   1   | Powers ESP32                              |
|            | Step‑down converter    | LM2596 (7‑9 V output)     |   1   | Powers motor drivers                      |
|            | Battery                | 2S Li‑ion (7.4 V)         |   1   | Or any battery capable of delivering ≥2 A |
| **Comm**   | WiFi                   | 2.4 GHz IEEE 802.11 b/g/n |   –   | ESP‑NOW and the UDP bridge                |

## Pin Configuration


| Name            | Component            |   №   |  Pin | Board Network          |
| --------------- | -------------------- | :---: | ---: | :--------------------- |
| **Motor Left**  | DRV8871 module       |   1   |  GND | Devboard‑GND           |
|                 |                      |   2   |   VM | LM2596 OUT+ (7‑9 V)    |
|                 |                      |   3   |  IN1 | Devboard‑GPIO_32       |
|                 |                      |   4   |  IN2 | Devboard‑GPIO_33       |
|                 |                      |       |      |                        |
| **Motor Right** | DRV8871 module       |   1   |  GND | Devboard‑GND           |
|                 |                      |   2   |   VM | LM2596 OUT+ (7‑9 V)    |
|                 |                      |   3   |  IN1 | Devboard‑GPIO_25       |
|                 |                      |   4   |  IN2 | Devboard‑GPIO_26       |
|                 |                      |       |      |                        |
| **Encoder L**   | JGA25 (Hall encoder) |   1   |  VCC | Devboard‑3V3           |
|                 |                      |   2   |  GND | Devboard‑GND           |
|                 |                      |   3   |    A | Devboard‑GPIO_36 (SVP) |
|                 |                      |   4   |    B | Devboard‑GPIO_39 (SVN) |
|                 |                      |       |      |                        |
| **Encoder R**   | JGA25 (Hall encoder) |   1   |  VCC | Devboard‑3V3           |
|                 |                      |   2   |  GND | Devboard‑GND           |
|                 |                      |   3   |    A | Devboard‑GPIO_35       |
|                 |                      |   4   |    B | Devboard‑GPIO_34       |

<blockquote>

### Notes

- JGA25 Encoder color phases are *A - Yellow*, *B - Green*
- All encoder signals are 3.3 V safe (JGA25 encoders include on‑board pull‑ups).  
- Motor drivers are powered from a separate **LM2596** step‑down converter set to **7‑9 V**.  
- ESP32 itself is powered from a **Mini 560 Pro** (5 V) connected to the `VIN` pin.  
- `NC` = *not connected* (not used in this table, all listed pins are connected).

</blockquote>

---

## License

This project is licensed under the **GNU General Public License v3.0 or later** (SPDX: `GPL-3.0-or-later`).

See the [LICENSE](LICENSE) file for the full text.
