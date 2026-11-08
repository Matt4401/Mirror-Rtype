#include "net/RTypeNetworkPlugin.hpp"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "log/Logger.hpp"
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace client {

namespace log = engine::log;
namespace protocol = rtype::protocol;

namespace {

constexpr const char* LOG_TAG = "client";

/**
 * @brief What to tell the player when the connection ends.
 * @param reason Why the transport closed.
 * @return Message for the player.
 */
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

RTypeNetworkPlugin::RTypeNetworkPlugin(std::shared_ptr<ecs::net::ClientConnection> connection, TransportFactory factory)
    : _connection(std::move(connection)), _factory(std::move(factory)) {}

void RTypeNetworkPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

template <typename Event, typename Handler>
void RTypeNetworkPlugin::listen(engine::Engine& engine, Handler handler) {
    ecs::EventBus& bus = engine.eventBus();
    const auto token = bus.subscribe<Event>(std::move(handler));
    _unsubscribe.emplace_back([&bus, token] { bus.unsubscribe<Event>(token); });
}

void RTypeNetworkPlugin::init(engine::Engine& engine) {
    listen<ecs::net::PeerConnected>(
        engine, [&engine](const ecs::net::PeerConnected& /*event*/) { sendMessage(engine, protocol::Hello{}); });
    listen<ecs::net::PeerDisconnected>(
        engine, [this, &engine](const ecs::net::PeerDisconnected& /*event*/) { onDisconnected(engine); });
    listen<ecs::net::PacketReceived>(
        engine, [this, &engine](const ecs::net::PacketReceived& packet) { onPacket(engine, packet); });

    listen<ConnectRequest>(engine, [this, &engine](const ConnectRequest& request) { onConnect(engine, request); });
    listen<DisconnectRequest>(engine, [this, &engine](const DisconnectRequest& /*request*/) {
        if (_state != SessionState::Offline) {
            close(engine, {}, false);
        }
    });
    listen<RefreshRoomsRequest>(engine, [this, &engine](const RefreshRoomsRequest& /*request*/) {
        if (_state == SessionState::Lobby || _state == SessionState::InRoom) {
            sendMessage(engine, protocol::LobbyListRequest{});
        }
    });
    listen<CreateRoomRequest>(engine, [this, &engine](const CreateRoomRequest& /*request*/) {
        if (_state == SessionState::Lobby) {
            sendMessage(engine, protocol::LobbyCreate{});
        }
    });
    listen<JoinRoomRequest>(engine, [this, &engine](const JoinRoomRequest& request) {
        if (_state == SessionState::Lobby) {
            sendMessage(engine, protocol::LobbyJoin{.roomId = request.roomId});
        }
    });
    listen<SetReadyRequest>(engine, [this, &engine](const SetReadyRequest& request) {
        if (_state == SessionState::InRoom) {
            sendMessage(engine, protocol::LobbyReady{.ready = request.ready});
        }
    });
    listen<LeaveRoomRequest>(engine, [this, &engine](const LeaveRoomRequest& /*request*/) {
        if (_state != SessionState::InRoom && _state != SessionState::InGame) {
            return;
        }
        sendMessage(engine, protocol::Leave{});
        leaveGame(engine);
        _state = SessionState::Lobby;
        _room.reset();
        engine.eventBus().publish(RoomLeft{});
    });
}

void RTypeNetworkPlugin::onConnect(engine::Engine& engine, const ConnectRequest& request) {
    if (_state != SessionState::Offline) {
        return;
    }
    const std::shared_ptr<ecs::net::INetworkClient> transport = _factory ? _factory() : nullptr;
    if (!transport) {
        close(engine, "no network available", true);
        return;
    }
    _connection->setClient(transport);
    _state = SessionState::Connecting;
    _lastError.clear();
    log::info(LOG_TAG, "connecting to ", request.host, ':', request.port);
    if (!transport->connect(request.host, request.port)) {
        close(engine, "the connection could not be started", true);
    }
}

void RTypeNetworkPlugin::onDisconnected(engine::Engine& engine) {
    if (_state == SessionState::Offline) {
        return;
    }
    const auto& transport = _connection->client();
    const std::string reason =
        transport ? describe(transport->disconnectReason()) : std::string("connection to the server lost");
    close(engine, reason, _state == SessionState::Connecting);
}

void RTypeNetworkPlugin::close(engine::Engine& engine, const std::string& reason, const bool duringConnection) {
    if (_state != SessionState::Offline && _state != SessionState::Connecting && reason.empty()) {
        sendMessage(engine, protocol::Leave{});
    }
    leaveGame(engine);
    if (const auto& transport = _connection->client()) {
        transport->stop();
        _retired = transport;
    }
    _connection->setClient(nullptr);
    _state = SessionState::Offline;
    _room.reset();
    _rooms = {};
    if (!reason.empty()) {
        log::error(LOG_TAG, reason);
        _lastError = reason;
        engine.eventBus().publish(SessionClosed{.duringConnection = duringConnection, .reason = reason});
    }
}

void RTypeNetworkPlugin::leaveGame(engine::Engine& engine) {
    if (!_playerIndex) {
        return;
    }
    if (const auto& transport = _connection->client()) {
        transport->closeUdp();
    }
    engine.getPlugin<ecs::net::ClientReplicationPlugin>().reset(engine);
    _playerIndex.reset();
    _level.reset();
}

void RTypeNetworkPlugin::onPacket(engine::Engine& engine, const ecs::net::PacketReceived& packet) {
    if (packet.channel != ecs::net::Channel::Reliable) {
        return;
    }
    if (const auto message = protocol::decode(packet.data)) {
        onMessage(engine, *message);
    }
}

void RTypeNetworkPlugin::onMessage(engine::Engine& engine, const protocol::Message& message) {
    ecs::EventBus& bus = engine.eventBus();
    if (const auto* welcome = std::get_if<protocol::Welcome>(&message)) {
        if (_state != SessionState::Connecting) {
            return;
        }
        _udpPort = welcome->udpPort;
        _state = SessionState::Lobby;
        bus.publish(SessionConnected{});
        sendMessage(engine, protocol::LobbyListRequest{});
    } else if (const auto* list = std::get_if<protocol::LobbyList>(&message)) {
        _rooms = *list;
        bus.publish(RoomListUpdated{.list = *list});
    } else if (const auto* update = std::get_if<protocol::LobbyUpdate>(&message)) {
        if (_state != SessionState::Lobby && _state != SessionState::InRoom) {
            return;
        }
        _state = SessionState::InRoom;
        _room = *update;
        bus.publish(RoomUpdated{.room = *update});
    } else if (const auto* start = std::get_if<protocol::GameStart>(&message)) {
        onGameStart(engine, *start);
    } else if (const auto* level = std::get_if<protocol::LevelStart>(&message)) {
        if (_state == SessionState::InGame) {
            _level = *level;
            bus.publish(LevelStarted{.level = *level});
        }
    } else if (const auto* complete = std::get_if<protocol::LevelComplete>(&message)) {
        if (_state == SessionState::InGame) {
            bus.publish(LevelCompleted{.result = *complete});
        }
    } else if (const auto* over = std::get_if<protocol::GameOver>(&message)) {
        if (_state != SessionState::InGame) {
            return;
        }
        leaveGame(engine);
        _state = SessionState::Lobby;
        _room.reset();
        _result = *over;
        bus.publish(GameEnded{.result = *over});
    } else if (const auto* left = std::get_if<protocol::PlayerLeft>(&message)) {
        bus.publish(PlayerLeftRoom{.clientId = left->clientId});
    }
}

void RTypeNetworkPlugin::onGameStart(engine::Engine& engine, const protocol::GameStart& start) {
    if (_state != SessionState::InRoom) {
        return;
    }
    const auto& transport = _connection->client();
    if (!transport || !transport->openUdp(_udpPort)) {
        close(engine, "cannot open UDP on port " + std::to_string(_udpPort), false);
        return;
    }
    log::info(LOG_TAG, "game started: player ", static_cast<int>(start.playerIndex) + 1, " of ",
              static_cast<int>(start.playerCount));
    _state = SessionState::InGame;
    _playerIndex = start.playerIndex;
    _result.reset();
    engine.eventBus().publish(GameStarted{.playerIndex = start.playerIndex, .playerCount = start.playerCount});
}

void RTypeNetworkPlugin::update(engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) { _retired.reset(); }

void RTypeNetworkPlugin::shutdown(engine::Engine& engine) {
    // The service is still there: NetworkPlugin, a dependency, shuts down after this plugin.
    if (_state != SessionState::Offline && _state != SessionState::Connecting) {
        sendMessage(engine, protocol::Leave{});
    }
    for (const auto& unsubscribe : _unsubscribe) {
        unsubscribe();
    }
    _unsubscribe.clear();
    _retired.reset();
}

void RTypeNetworkPlugin::sendMessage(engine::Engine& engine, const protocol::Message& message) {
    engine.service<ecs::net::INetworkService>().send(ecs::net::SERVER_PEER, protocol::encode(message),
                                                     ecs::net::Channel::Reliable);
}

}  // namespace client
