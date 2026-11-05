#include "ServerManager.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>
#include <variant>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "Room.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace rtype::server {

namespace {

// Sleep between two loop iterations. Short enough to keep the step jitter around 1 ms, long enough not to burn a core.
constexpr auto IDLE_SLEEP = std::chrono::milliseconds(1);

}  // namespace

ServerManager::ServerManager(std::unique_ptr<ecs::net::INetworkServer> network,
                             const std::chrono::milliseconds handshakeTimeout)
    : _network(std::move(network)), _handshakeTimeout(handshakeTimeout) {}

bool ServerManager::run(const std::uint16_t port) {
    if (!_network->start(port)) {
        return false;
    }
    _port = port;
    log::info(LOG_TAG, "listening on port ", port);
    log::debug(LOG_TAG, "debug logs enabled");
    _running = true;

    while (_running.load()) {
        handleEvents();
        expireHandshakes();
        std::this_thread::sleep_for(IDLE_SLEEP);
    }

    // Rooms running in the background send through the network: they stop first.
    _rooms.clear();
    _roomOf.clear();
    _network->stop();
    log::info(LOG_TAG, "stopped");
    return true;
}

void ServerManager::stop() { _running = false; }

void ServerManager::handleEvents() {
    for (const auto& [type, client, data, channel] : _network->poll()) {
        switch (type) {
            case ecs::net::EventType::Connected:
                onConnected(client);
                break;
            case ecs::net::EventType::Disconnected:
                onDisconnected(client);
                break;
            case ecs::net::EventType::Data:
                onData(client, channel, data);
                break;
        }
    }
}

void ServerManager::onConnected(const ClientId client) {
    _pendingHandshakes[client] = std::chrono::steady_clock::now();
    log::info(LOG_TAG, "client ", client, " connected");
}

void ServerManager::onDisconnected(const ClientId client) {
    // Forgotten first: leaveRoom() pushes the list of rooms, which this client must not receive.
    _handshaken.erase(client);
    _pendingHandshakes.erase(client);
    // Can arrive without onConnected (connection lost before being ready): leaveRoom() then does nothing.
    leaveRoom(client);
    log::info(LOG_TAG, "client ", client, " disconnected");
}

void ServerManager::onData(const ClientId client, const ecs::net::Channel channel, const ecs::net::Bytes& data) {
    if (channel == ecs::net::Channel::Unreliable) {
        onUnreliableData(client, data);
        return;
    }
    const auto message = protocol::decode(data);
    if (!message) {
        log::debug(LOG_TAG, "client ", client, " sent an invalid payload of ", data.size(), " bytes: dropped");
        return;
    }
    onReliableMessage(client, *message);
}

void ServerManager::onReliableMessage(const ClientId client, const protocol::Message& message) {
    if (const auto* hello = std::get_if<protocol::Hello>(&message)) {
        if (_handshaken.contains(client)) {
            log::debug(LOG_TAG, "client ", client, " sent a second Hello: dropped");
            return;
        }
        if (hello->version != protocol::PROTOCOL_VERSION) {
            log::info(LOG_TAG, "client ", client, " uses protocol version ", hello->version, " (expected ",
                      protocol::PROTOCOL_VERSION, "): refused");
            _pendingHandshakes.erase(client);
            _network->refuse(client, ecs::net::DisconnectReason::ProtocolMismatch);
            return;
        }
        _pendingHandshakes.erase(client);
        _handshaken.insert(client);
        send(client, protocol::Welcome{.udpPort = _port}, ecs::net::Channel::Reliable);
        return;
    }
    if (!_handshaken.contains(client)) {
        log::debug(LOG_TAG, "client ", client, " sent a message before Hello: dropped");
        return;
    }
    if (std::holds_alternative<protocol::Leave>(message)) {
        leaveRoom(client);
        return;
    }
    if (isInGame(client)) {
        // TODO(protocol): Chat -> fill clientId, then send to the other players of the room (also valid in the lobby).
        log::debug(LOG_TAG, "client ", client, " sent a message out of phase (index ", message.index(), "): dropped");
        return;
    }
    if (std::holds_alternative<protocol::LobbyListRequest>(message)) {
        send(client, lobbyList(), ecs::net::Channel::Reliable);
    } else if (std::holds_alternative<protocol::LobbyCreate>(message)) {
        onLobbyCreate(client);
    } else if (const auto* join = std::get_if<protocol::LobbyJoin>(&message)) {
        onLobbyJoin(client, join->roomId);
    } else if (const auto* ready = std::get_if<protocol::LobbyReady>(&message)) {
        onLobbyReady(client, ready->ready);
    } else {
        log::debug(LOG_TAG, "client ", client, " sent an unhandled message (index ", message.index(), ")");
    }
}

void ServerManager::onUnreliableData(const ClientId client, const ecs::net::Bytes& data) {
    if (!_handshaken.contains(client)) {
        log::debug(LOG_TAG, "client ", client, " sent UDP data before Hello: dropped");
        return;
    }
    const auto id = roomOf(client);
    if (!id) {
        log::debug(LOG_TAG, "client ", client, " is not in a room: UDP data ignored");
        return;
    }
    if (const auto room = _rooms.find(*id); room != _rooms.end()) {
        room->second.onPacket(client, data);
    }
}

void ServerManager::expireHandshakes() {
    const auto now = std::chrono::steady_clock::now();
    // A kicked client leaves the map at once: it is kicked only once, even if its Disconnected event arrives a few
    // iterations later.
    std::erase_if(_pendingHandshakes, [this, now](const auto& pending) {
        const auto& [client, connectedAt] = pending;
        if (now - connectedAt < _handshakeTimeout) {
            return false;
        }
        log::info(LOG_TAG, "client ", client, " sent no Hello within ", _handshakeTimeout.count(), " ms: kicked");
        _network->kick(client);
        return true;
    });
}

void ServerManager::send(const ClientId client, const protocol::Message& message,
                         const ecs::net::Channel channel) const {
    _network->send(client, protocol::encode(message), channel);
}

void ServerManager::onLobbyCreate(const ClientId client) {
    if (roomOf(client) || _rooms.size() >= protocol::MAX_ROOMS) {
        log::debug(LOG_TAG, "client ", client, " cannot create a room: dropped");
        return;
    }
    const RoomId id = _nextRoomId++;
    Room& room = _rooms.try_emplace(id, *_network, MAX_PLAYERS_PER_ROOM).first->second;
    room.addPlayer(client);
    _roomOf[client] = id;
    log::debug(LOG_TAG, "client ", client, " created room ", id);
    sendLobbyUpdate(id, room);
    broadcastLobbyList();
}

void ServerManager::onLobbyJoin(const ClientId client, const RoomId id) {
    const auto it = _rooms.find(id);
    if (roomOf(client) || it == _rooms.end() || !it->second.addPlayer(client)) {
        log::debug(LOG_TAG, "client ", client, " cannot join room ", id, ": dropped");
        return;
    }
    _roomOf[client] = id;
    log::debug(LOG_TAG, "client ", client, " joined room ", id, " (", it->second.players().size(), '/',
               static_cast<int>(MAX_PLAYERS_PER_ROOM), " players)");
    sendLobbyUpdate(id, it->second);
    broadcastLobbyList();
}

void ServerManager::onLobbyReady(const ClientId client, const bool ready) {
    const auto id = roomOf(client);
    if (!id) {
        log::debug(LOG_TAG, "client ", client, " is not in a room: LobbyReady dropped");
        return;
    }
    const auto it = _rooms.find(*id);
    if (it == _rooms.end()) {
        return;
    }
    it->second.setReady(client, ready);
    sendLobbyUpdate(*id, it->second);
    startIfReady(*id, it->second);
}

void ServerManager::leaveRoom(const ClientId client) {
    const auto id = roomOf(client);
    if (!id) {
        return;
    }
    _roomOf.erase(client);

    const auto it = _rooms.find(*id);
    if (it == _rooms.end()) {
        return;
    }
    Room& room = it->second;
    room.removePlayer(client);
    log::debug(LOG_TAG, "client ", client, " left room ", *id);
    if (room.isEmpty()) {
        _rooms.erase(it);
        log::debug(LOG_TAG, "room ", *id, " destroyed (empty)");
        broadcastLobbyList();
        return;
    }
    const auto left = protocol::encode(protocol::PlayerLeft{.clientId = client});
    for (const RoomPlayer& other : room.players()) {
        _network->send(other.client, left, ecs::net::Channel::Reliable);
    }
    if (!room.isPlaying()) {
        sendLobbyUpdate(*id, room);
        startIfReady(*id, room);
    }
    broadcastLobbyList();
}

void ServerManager::startIfReady(const RoomId id, Room& room) {
    if (room.isPlaying() || !room.isEveryoneReady()) {
        return;
    }
    room.start();
    room.runInBackground();
    log::info(LOG_TAG, "room ", id, " started with ", room.players().size(), " players");
    const auto count = static_cast<std::uint8_t>(room.players().size());
    std::uint8_t index = 0;
    for (const RoomPlayer& player : room.players()) {
        send(player.client, protocol::GameStart{.roomId = id, .playerIndex = index, .playerCount = count},
             ecs::net::Channel::Reliable);
        ++index;
    }
    broadcastLobbyList();
}

void ServerManager::sendLobbyUpdate(const RoomId id, const Room& room) const {
    protocol::LobbyUpdate update{.roomId = id, .slots = {}};
    update.slots.reserve(room.players().size());
    for (const RoomPlayer& player : room.players()) {
        update.slots.push_back({.clientId = player.client, .ready = player.ready});
    }
    const auto bytes = protocol::encode(update);
    for (const RoomPlayer& player : room.players()) {
        _network->send(player.client, bytes, ecs::net::Channel::Reliable);
    }
}

protocol::LobbyList ServerManager::lobbyList() const {
    protocol::LobbyList list;
    list.rooms.reserve(_rooms.size());
    for (const auto& [id, room] : _rooms) {
        list.rooms.push_back({
            .roomId = id,
            .playerCount = static_cast<std::uint8_t>(room.players().size()),
            .maxPlayers = MAX_PLAYERS_PER_ROOM,
            .state = room.isPlaying() ? protocol::RoomState::InGame : protocol::RoomState::Waiting,
        });
    }
    return list;
}

void ServerManager::broadcastLobbyList() const {
    const auto bytes = protocol::encode(lobbyList());
    for (const ClientId client : _handshaken) {
        if (!isInGame(client)) {
            _network->send(client, bytes, ecs::net::Channel::Reliable);
        }
    }
}

bool ServerManager::isInGame(const ClientId client) const {
    const auto id = roomOf(client);
    if (!id) {
        return false;
    }
    const auto it = _rooms.find(*id);
    return it != _rooms.end() && it->second.isPlaying();
}

std::optional<RoomId> ServerManager::roomOf(const ClientId client) const {
    const auto it = _roomOf.find(client);
    if (it == _roomOf.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace rtype::server
