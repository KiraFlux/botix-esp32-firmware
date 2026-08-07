// Copyright (c) 2026 KiraFlux
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include <kf/BytesView.hpp>
#include <kf/Logger.hpp>
#include <kf/MacAddress.hpp>
#include <kf/NoneType.hpp>
#include <kf/Option.hpp>
#include <kf/esp/Espnow.hpp>

#include <kf/mixin/Initable.hpp>
#include <kf/mixin/NonCopyable.hpp>

#include "botix/transport/Address.hpp"
#include "botix/transport/Transport.hpp"

namespace botix::transport {

struct EspnowTransport final :

    Transport,
    kf::mixin::Initable<EspnowTransport, bool()>

{
    using Self = EspnowTransport;

    constexpr EspnowTransport() noexcept :
        Transport{Kind::Espnow} {}

    bool send(kf::BytesView buffer) noexcept override {
        if (_active_peer.isSome()) {
            return _active_peer.unwrap().writeBuffer(buffer).isOk();
        } else {
            return false;
        }
    }

protected:
    bool doConnect(Address const &address) noexcept override {
        auto peer_result = Espnow::Peer::create({
            .mac_address = address.mac(),
            .wifi_interface_sta = true,
        });

        if (peer_result.isError()) {
            logger.error("Espnow peer add failed: {}", peer_result.error());
            return false;
        }

        _active_peer = kf::some(std::move(peer_result.ok()));
        return true;
    }

    void doDisconnect() noexcept override {
        if (not connected()) {
            logger.warn("Disconnect failed: No active peer");
            return;
        }

        auto &peer = _active_peer.unwrap();
        if (not peer.exist()) {
            logger.error("Disconnect failed: Peer not exit");
            return;
        }

        _active_peer.reset();
        logger.info("Disconnected: OK");
    }

private:
    using Espnow = kf::esp::Espnow;

    inline static char logger_buffer[64];

    inline static kf::Logger logger{"EspnowTransport", {logger_buffer}};

    kf::Option<Espnow::Peer> _active_peer{kf::none};

    KF_IMPL_INITABLE(Self, bool());
    bool initImpl() noexcept {
        auto &espnow = Espnow::instance();

        auto const init_result = espnow.init();
        if (init_result.isError()) {
            logger.error("Espnow init failed: {}", init_result.error());
            return false;
        }

        espnow.callback([this](kf::MacAddress const &mac_address, kf::BytesView buffer) -> void {
            if (_active_peer.isSome() and _active_peer.unwrap().mac() == mac_address) {
                this->invokeReceiveCallback(buffer);
            } else {
                this->invokeReceiveForeignCallback(Address::createForEspnow(mac_address), buffer);
            }
        });

        return true;
    }
};

}// namespace botix::transport