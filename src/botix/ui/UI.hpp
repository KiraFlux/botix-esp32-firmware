// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/ui/Event.hpp>
#include <kf/ui/UI.hpp>
#include <kf/ui/render/PlainTextRender.hpp>

namespace botix::ui {

using UI = ::kf::ui::UI<::kf::ui::render::PlainTextRender<128>, ::kf::ui::Event<6>>;

}// namespace botix::ui