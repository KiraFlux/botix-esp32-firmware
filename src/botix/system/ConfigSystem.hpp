// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <kf/Arena.hpp>
#include <kf/Bytes.hpp>
#include <kf/Timer.hpp>
#include <kf/core.hpp>
#include <kf/esp/NVS.hpp>
#include <kf/units.hpp>

#include "botix/cli/Group.hpp"
#include "botix/config/Config.hpp"
#include "botix/config/DeviceConfig.hpp"
#include "botix/config/UserConfig.hpp"
#include "botix/service/ConfigService.hpp"

#include "botix/system/System.hpp"

namespace botix::system {

struct ConfigSystem : System<ConfigSystem> {

    template<kf::implements<config::ConfigTag> ConfigImpl> struct Strategy {

        static void load(kf::Bytes bytes) noexcept {
            auto maybe_config = ConfigImpl::fromBytes(bytes);
            if (maybe_config.isNone()) { return; }

            auto &config = maybe_config.unwrap();
            if (config.version != ConfigImpl::latest_version) {
                config.reset();
            }
        }

        static void reset(kf::Bytes bytes) noexcept {
            if (auto maybe_config = ConfigImpl::fromBytes(bytes); maybe_config.isSome()) {
                maybe_config.unwrap().reset();
            }
        }

        static void init(service::ConfigService &service) noexcept {
            service.onLoad(load);
            service.resettingStrategy(reset);
            service.requestLoad();
            service.sync();
        }
    };

    constexpr ConfigSystem() noexcept :
        System<ConfigSystem>{{.name{"config"}}} {};

private:
    static constexpr kf::Timer::Config _sync_timer_config{.value = 10'000};

    kf::esp::NVS _nvs{"botix"};

public:
    config::DeviceConfig device{};
    config::UserConfig user{};

    service::ConfigService device_service{{
        .nvs = _nvs,
        .key = "dev-cfg",
        .sync_timer_config = _sync_timer_config,
        .config_bytes = device.bytes(),
    }};

    service::ConfigService user_service{{
        .nvs = _nvs,
        .key = "usr-cfg",
        .sync_timer_config = _sync_timer_config,
        .config_bytes = user.bytes(),
    }};

private:
    BOTIX_IMPL_SYSTEM(ConfigSystem);

    void onSetupImpl() noexcept {
        if (_nvs.init().isError()) {
            // _logger.error("NVS init failed");
        }

        Strategy<config::DeviceConfig>::init(device_service);
        Strategy<config::UserConfig>::init(user_service);
    }

    void setupCliImpl(kf::Arena &arena, cli::Group &group) noexcept {
        // value <path> [lexeme: str=""]
        //
        // try to resolve path
        // if lexeme is empty => get
        // try to parse lexeme as field value

        // reset <group-name>
    }

    void pollImpl(kf::units::Milliseconds now) noexcept {
        device_service.poll(now);
        user_service.poll(now);
    }
};

}// namespace botix::system