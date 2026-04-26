// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "botix/ui/UI.hpp"

namespace botix::ui {

struct RootPage final : UI::Page {

    constexpr RootPage() noexcept : Page{"Root"} {
        widgets({_layout.data(), _layout.size()});

        _test_log_button.callback([this]() {
            logger.info("test log button");
            _page_value += 1;
            _value_display.value(_page_value);
        });
    }

    void onEntry() noexcept override {
        logger.debug("entry");
    }

    void onExit() noexcept override {
        logger.debug("exit");
    }

private:
    static constexpr auto logger{kf::Logger::create("RootPage")};

    int _page_value{123};

    // widgets

    UI::Button _test_log_button{"Test Log"};
    UI::Display<int> _value_display{_page_value};

    // layout

    kf::memory::Array<UI::Widget *, 2> _layout{{
        &_test_log_button,
        &_value_display,
    }};
};

}// namespace botix::ui