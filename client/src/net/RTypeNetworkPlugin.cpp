#include "net/RTypeNetworkPlugin.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <variant>

#include "Logger.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/LocalInput.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace client {

namespace log = rtype::log;
namespace protocol = rtype::protocol;

namespace {

constexpr const char* LOG_TAG = "client";

/** @brief What to tell the player when the connection ends. */
constexpr const char* describe(const ecs::net::DisconnectReason reason) {
    switch (reason) {
        case ecs::net::DisconnectReason::ProtocolMismatch:
            return "the server runs another version of the game";
        case ecs::net::DisconnectReason::ServerFull:
            return "the server is full";
        case ecs::net::DisconnectReason::Unreachable:
            return "the server cannot be reached";
        case ecs::net::DisconnectReason::ConnectionLost:
            break;
    }
    return "connection to the server lost";
}

}  // namespace

RTypeNetworkPlugin::RTypeNetworkPlugin(std::shared_ptr<ecs::net::INetworkClient> transport)
    : _transport(std::move(transport)) {}

void RTypeNetworkPlugin::build(engine::PluginBuilder& builder) { builder.require<ecs::net::NetworkPlugin>(); }

void RTypeNetworkPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _connected = events.subscribe<ecs::net::PeerConnected>(
        [&engine](const ecs::net::PeerConnected& /*event*/) { sendMessage(engine, protocol::Hello{}); });
    _disconnected = events.subscribe<ecs::net::PeerDisconnected>(
        [this, &engine](const ecs::net::PeerDisconnected& /*event*/) { onDisconnected(engine); });
    _packets = events.subscribe<ecs::net::PacketReceived>(
        [this, &engine](const ecs::net::PacketReceived& packet) { onPacket(engine, packet); });
}

void RTypeNetworkPlugin::onDisconnected(engine::Engine& engine) {
    log::error(LOG_TAG, describe(_transport->disconnectReason()));
    _failed = true;
    engine.stop();
}

void RTypeNetworkPlugin::onPacket(engine::Engine& engine, const ecs::net::PacketReceived& packet) {
    if (packet.channel != ecs::net::Channel::Reliable) {
        return;
    }
    const auto message = protocol::decode(packet.data);
    if (!message) {
        return;
    }
    if (const auto* welcome = std::get_if<protocol::Welcome>(&*message)) {
        _udpPort = welcome->udpPort;
        sendMessage(engine, protocol::LobbyListRequest{});
    } else if (const auto* list = std::get_if<protocol::LobbyList>(&*message)) {
        chooseRoom(engine, *list);
    } else if (const auto* update = std::get_if<protocol::LobbyUpdate>(&*message)) {
        if (!_room) {
            log::info(LOG_TAG, "in room ", update->roomId, ": press R or Start when everyone is here");
        }
        _room = update->roomId;
        const auto ready = std::ranges::count_if(update->slots, &protocol::PlayerSlot::ready);
        log::info(LOG_TAG, "room ", update->roomId, ": ", ready, '/', update->slots.size(), " players ready");
    } else if (const auto* start = std::get_if<protocol::GameStart>(&*message)) {
        onGameStart(engine, *start);
    } else if (const auto* left = std::get_if<protocol::PlayerLeft>(&*message)) {
        log::info(LOG_TAG, "player ", left->clientId, " left");
    }
}

void RTypeNetworkPlugin::onGameStart(engine::Engine& engine, const protocol::GameStart& start) {
    if (!_transport->openUdp(_udpPort)) {
        log::error(LOG_TAG, "cannot open UDP on port ", _udpPort);
        _failed = true;
        engine.stop();
        return;
    }
    log::info(LOG_TAG, "game started: player ", static_cast<int>(start.playerIndex) + 1, " of ",
              static_cast<int>(start.playerCount));
    _inGame = true;
    engine.eventBus().publish(GameStarted{.playerIndex = start.playerIndex});
}

void RTypeNetworkPlugin::chooseRoom(engine::Engine& engine, const protocol::LobbyList& list) const {
    if (_room) {
        return;
    }
    const auto open = std::ranges::find_if(list.rooms, [](const protocol::RoomEntry& room) {
        return room.state == protocol::RoomState::Waiting && room.playerCount < room.maxPlayers;
    });
    if (open == list.rooms.end()) {
        sendMessage(engine, protocol::LobbyCreate{});
    } else {
        sendMessage(engine, protocol::LobbyJoin{.roomId = open->roomId});
    }
}

void RTypeNetworkPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    if (!_room || _inGame) {
        return;
    }
    bool pressed = false;
    withLocalInput(engine.registry(), [&pressed](const ::input::InputState& state) {
        pressed = state.isJustPressed(input::ACTION_READY);
    });
    if (pressed) {
        _ready = !_ready;
        sendMessage(engine, protocol::LobbyReady{.ready = _ready});
    }
}

void RTypeNetworkPlugin::shutdown(engine::Engine& engine) {
    // The service is still there: NetworkPlugin, a dependency, shuts down after this plugin.
    sendMessage(engine, protocol::Leave{});
    auto& events = engine.eventBus();
    if (_connected) {
        events.unsubscribe<ecs::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    if (_disconnected) {
        events.unsubscribe<ecs::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
    if (_packets) {
        events.unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
}

void RTypeNetworkPlugin::sendMessage(engine::Engine& engine, const protocol::Message& message) {
    engine.service<ecs::net::INetworkService>().send(ecs::net::SERVER_PEER, protocol::encode(message),
                                                     ecs::net::Channel::Reliable);
}

}  // namespace client
