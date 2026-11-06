#include "LobbyPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "Logger.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::server {

namespace {

constexpr const char* LOG_TAG = "lobby";

}  // namespace

void LobbyPlugin::build(engine::PluginBuilder& builder) { builder.require<ecs::net::NetworkPlugin>(); }

void LobbyPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _connected = events.subscribe<ecs::net::PeerConnected>([this, &engine](const ecs::net::PeerConnected& event) {
        _players.insert(event.peer);
        rtype::log::info(LOG_TAG, "player ", event.peer, " joined (", _players.size(), " connected)");
        broadcastState(engine);
    });
    _disconnected =
        events.subscribe<ecs::net::PeerDisconnected>([this, &engine](const ecs::net::PeerDisconnected& event) {
            if (_players.erase(event.peer) == 0) {
                return;
            }
            rtype::log::info(LOG_TAG, "player ", event.peer, " left (", _players.size(), " connected)");
            broadcastState(engine);
        });
}

void LobbyPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_connected) {
        events.unsubscribe<ecs::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    if (_disconnected) {
        events.unsubscribe<ecs::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
    _players.clear();
}

void LobbyPlugin::broadcastState(engine::Engine& engine) const {
    const auto players =
        static_cast<std::uint16_t>(std::min<std::size_t>(_players.size(), std::numeric_limits<std::uint16_t>::max()));
    engine.service<ecs::net::INetworkService>().broadcast(protocol::encode(protocol::LobbyState{.players = players}),
                                                          ecs::net::Channel::Reliable);
}

}  // namespace rlss::server
