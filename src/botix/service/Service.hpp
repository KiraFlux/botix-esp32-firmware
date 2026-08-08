// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/mixin/NonCopyable.hpp>
#include <kf/mixin/TimedPollable.hpp>

namespace botix::service {

struct ServiceTag {};

template<typename Impl> struct Service :

    ServiceTag,
    kf::mixin::NonCopyable,
    kf::mixin::TimedPollable<Impl>

{};

}// namespace botix::service

#define BOTIX_IMPL_SERVICE(...) KF_IMPL_TIMED_POLLABLE(__VA_ARGS__)
