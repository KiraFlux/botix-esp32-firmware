# Botix: ESP32 Mobile Robot Firmware

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Modular ESP32 firmware for the [**Botix** open‑source mobile robot platform](https://github.com/KiraFlux/Botix.git)

Currently used for hardware bring‑up and basic remote control tests; the final target is a transparent **WiFi (TCP/UDP) bridge** that exposes the robot's low‑level capabilities to a higher‑level controller (ROS 2 node, PC application, etc.).

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

## Current State

<blockquote>

**Temporary test mode** – the robot is controlled via **ESP‑NOW** using an [ESP32 Dual Joystick Controller (DJC)](https://github.com/KiraFlux/ESP32-DJC).  
This is only intended for hardware validation and will be replaced by a permanent WiFi (UDP/TCP) command & telemetry interface.

</blockquote>

## Planned Features

- WiFi UDP/TCP bridge for ROS 2 or other high‑level controllers
- High‑rate odometry telemetry (encoder deltas)
- On‑board voltage monitoring
- Persistent configuration storage (NVS)
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
| **Comm**   | WiFi                   | 2.4 GHz IEEE 802.11 b/g/n |   –   | Used for future UDP/TCP bridge            |

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
|                 |                      |   3   |    A | Devboard‑GPIO_34       |
|                 |                      |   4   |    B | Devboard‑GPIO_35       |

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