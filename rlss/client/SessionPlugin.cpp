#include "SessionPlugin.hpp"

#include <memory>
#include <utility>
#include <variant>

#include "Logger.hpp"
#include "ServerLink.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "client";

}  // namespace

SessionPlugin::SessionPlugin(std::shared_ptr<ecs::net::INetworkClient> transport) : _transport(std::move(transport)) {}

void SessionPlugin::build(engine::PluginBuilder& builder) { builder.require<ecs::net::NetworkPlugin>(); }

void SessionPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _disconnected =
        events.subscribe<ecs::net::PeerDisconnected>([this, &engine](const ecs::net::PeerDisconnected& /*event*/) {
            rtype::log::error(LOG_TAG, "disconnected: ", describe(_transport->disconnectReason()));
            _lost = true;
            engine.stop();
        });
    _packets = events.subscribe<ecs::net::PacketReceived>([](const ecs::net::PacketReceived& packet) {
        const auto message = protocol::decode(packet.data);
        if (!message) {
            return;  // untrusted bytes: a malformed message is ignored
        }
        if (const auto* lobby = std::get_if<protocol::LobbyState>(&*message)) {
            rtype::log::info(LOG_TAG, "players connected: ", lobby->players);
        }
    });
}

void SessionPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_disconnected) {
        events.unsubscribe<ecs::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
    if (_packets) {
        events.unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
}

}  // namespace rlss::client
