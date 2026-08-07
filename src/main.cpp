// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#include <kf/main.hpp>
#include <kf/rtos/Clock.hpp>
#include <kf/rtos/Task.hpp>

#include <kf/Logger.hpp>
#include <kf/units.hpp>

#include "botix/Control.hpp"
#include "botix/Periphery.hpp"
#include "botix/RootConfig.hpp"
#include "botix/transport/EspnowTransport.hpp"

static botix::RootConfig root_config{};

static botix::Periphery periphery{
    root_config.periphery,
};

static botix::Control control{
    root_config.control,
};

static botix::EspnowTransport espnow_transport{};

static void onTransportReceive(kf::MacAddress const &mac, kf::Slice<kf::u8 const> buffer) {
    switch (buffer.length()) {
        case sizeof(botix::Control::Input):
            control.input(*reinterpret_cast<botix::Control::Input const *>(buffer.data()));
            return;

        default:
            return;
    }
}

void kf::main(kf::Init &init) {
    init.logger.debug("Starting");

    root_config.reset();// set to defaults

    if (not periphery.init()) {
        init.logger.error("Periphery init failed");
        return;
    }

    if (not espnow_transport.init()) {
        init.logger.warn("EspnowTransport init failed");
        return;
    }

    espnow_transport.callback(onTransportReceive);

    init.logger.info("Ready");

    while (true) {
        constexpr auto loop_period{1000 / 100};// 10 Hz Loop rate

        auto const now = rtos::Clock::now();

        espnow_transport.poll(now);
        control.poll(now);

        {
            auto const &o = control.output();

            periphery.motor_driver_left.set(o.motor_left_set);
            periphery.motor_driver_right.set(o.motor_right_set);
        }

        {
            while (init.io.availableForRead() > 0) {
                if (auto const read = init.io.readPacket<char>(); read.isOk()) {
                    if (read.ok() == 'e') {
                        init.logger.debug(
                            "Encoders: \tL: {} \t R: {}",
                            periphery.wheel_odometry_encoder_left.positionTicks(),
                            periphery.wheel_odometry_encoder_right.positionTicks());
                    }
                }
            }
        }

        rtos::Task::sleep(loop_period);
    }
}
