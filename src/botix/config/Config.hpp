// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Bytes.hpp>
#include <kf/BytesView.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/primitives.hpp>

#include <kf/mixin/Resettable.hpp>

namespace botix::config {

struct ConfigTag {};

/// @brief Persistent configuration structure
template<typename Impl, kf::u8 latest_version> struct Config :

    ConfigTag,
    kf::mixin::Resettable<Impl>

{
    kf::u8 version;

    [[nodiscard]] constexpr bool isLatest() const noexcept {
        return version == latest_version;
    }

    [[nodiscard]] static constexpr auto interpret(kf::Bytes view) noexcept -> kf::Option<Impl &> {
        return (view.length() == sizeof(Impl)) ? kf::someRef(*reinterpret_cast<Impl *>(view.data())) : kf::none;
    }

    [[nodiscard]] constexpr auto view() noexcept -> kf::Bytes {
        return {
            reinterpret_cast<kf::u8 *>(this),
            sizeof(Impl),
        };
    }

    [[nodiscard]] constexpr auto view() const noexcept -> kf::BytesView {
        return const_cast<Impl *>(this)->view();
    }

    [[nodiscard]] static constexpr auto defaults() noexcept {
        Impl ret{};

        ret.version = latest_version;
        ret.reset();

        return ret;
    }
};

}// namespace botix::config