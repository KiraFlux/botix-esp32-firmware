// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

namespace botix::system {

template<typename Impl, typename InitSignature> struct System :

    kf::mixin::NonCopyable,
    kf::mixin::Initable<Impl, InitSignature>,
    kf::mixin::TimedPollable<Impl>

{};

}// namespace botix::system

#define BOTIX_IMPL_SYSTEM(__impl__, ...)     \
    KF_IMPL_INITABLE(__impl__, __VA_ARGS__); \
    KF_IMPL_TIMED_POLLABLE(__impl__)
