// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/primitives.hpp>

#include <kf/mixin/Resettable.hpp>

namespace botix::cli {

struct Config : kf::mixin::Resettable<Config> {

    kf::u8
        max_channel_count{0x08},
        max_namespace_count{0x10},
        max_command_count{0x10},
        max_command_argument_count{0x08};

    kf::u16
        channel_input_queue_length{0x02'00},
        channel_input_line_length{0x00'80},
        channel_output_line_length{0x00'80};

private:
    // TODO: extract as mixin::DefaultConstructResettable<Impl>
    KF_IMPL_RESETTABLE(Config);
    constexpr void resetImpl() noexcept {
        *this = Config{};
    }
};

}