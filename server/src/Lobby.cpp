#include "Lobby.hpp"

#include <cstdint>
#include <optional>
#include <variant>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "Room.hpp"
#include "ecs/time/Time.hpp"
#include "net/INetworkManager.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace rtype::server {

void Lobby::addClient(const ClientId client) { _clients.insert(client); }

void Lobby::removeClient(const ClientId client) {
    // Forgotten first: leaveRoom() pushes the list of rooms, which this client must not receive.
    _clients.erase(client);
    // Can arrive for a client in no room: leaveRoom() then does nothing.
    leaveRoom(client);
}

void Lobby::onMessage(const ClientId client, const protocol::Message& message) {
    if (isInGame(client)) {
        // TODO(protocol): Chat -> fill clientId, then send to the other players of the room (also valid in the lobby).
        log::debug(LOG_TAG, "client ", client, " sent a message out of phase (index ", message.index(), "): dropped");
        return;
    }
    if (std::holds_alternative<protocol::LobbyListRequest>(message)) {
        _network->send(client, protocol::encode(lobbyList()), ecs::net::Channel::Reliable);
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

void Lobby::onUnreliableData(const ClientId client, const ecs::net::Bytes& data) {
    const auto id = roomOf(client);
    if (!id) {
        log::debug(LOG_TAG, "client ", client, " is not in a room: UDP data ignored");
        return;
    }
    if (const auto room = _rooms.find(*id); room != _rooms.end()) {
        room->second.onPacket(client, data);
    }
}

void Lobby::clear() {
    const auto shutdown = protocol::encode(protocol::GameOver{.reason = protocol::GameOverReason::ServerShutdown});
    for (const auto& [id, room] : _rooms) {
        if (!room.isPlaying() || room.hasEnded()) {
            continue;
        }
        for (const RoomPlayer& player : room.players()) {
            _network->send(player.client, shutdown, ecs::net::Channel::Reliable);
        }
    }
    _rooms.clear();
    _roomOf.clear();
}

void Lobby::update() {
    bool reaped = false;
    for (auto it = _rooms.begin(); it != _rooms.end();) {
        if (!it->second.hasEnded()) {
            ++it;
            continue;
        }
        // The room already sent GameOver to its players: they are back in the lobby.
        for (const RoomPlayer& player : it->second.players()) {
            _roomOf.erase(player.client);
        }
        log::info(LOG_TAG, "room ", it->first, " ended");
        it = _rooms.erase(it);  // joins the room's thread
        reaped = true;
    }
    if (reaped) {
        broadcastLobbyList();
    }
}

void Lobby::onLobbyCreate(const ClientId client) {
    if (roomOf(client) || _rooms.size() >= protocol::MAX_ROOMS) {
        log::debug(LOG_TAG, "client ", client, " cannot create a room: dropped");
        return;
    }
    const RoomId id = _nextRoomId++;
    Room& room = _rooms.try_emplace(id, *_network, MAX_PLAYERS_PER_ROOM, ecs::time::DEFAULT_FIXED_DT, nullptr, _level)
                     .first->second;
    room.addPlayer(client);
    _roomOf[client] = id;
    log::debug(LOG_TAG, "client ", client, " created room ", id);
    sendLobbyUpdate(id, room);
    broadcastLobbyList();
}

void Lobby::onLobbyJoin(const ClientId client, const RoomId id) {
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

void Lobby::onLobbyReady(const ClientId client, const bool ready) {
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

void Lobby::leaveRoom(const ClientId client) {
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
        if (startIfReady(*id, room)) {
            return;  // it already sent the list of rooms
        }
    }
    broadcastLobbyList();
}

bool Lobby::startIfReady(const RoomId id, Room& room) {
    if (room.isPlaying() || !room.isEveryoneReady()) {
        return false;
    }
    // GameStart goes out first: starting the room sends the LevelStart of the first level.
    const auto count = static_cast<std::uint8_t>(room.players().size());
    std::uint8_t index = 0;
    for (const RoomPlayer& player : room.players()) {
        _network->send(player.client,
                       protocol::encode(protocol::GameStart{.roomId = id, .playerIndex = index, .playerCount = count}),
                       ecs::net::Channel::Reliable);
        ++index;
    }
    room.start();
    room.runInBackground();
    log::info(LOG_TAG, "room ", id, " started with ", room.players().size(), " players");
    broadcastLobbyList();
    return true;
}

void Lobby::sendLobbyUpdate(const RoomId id, const Room& room) const {
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

protocol::LobbyList Lobby::lobbyList() const {
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

void Lobby::broadcastLobbyList() const {
    const auto bytes = protocol::encode(lobbyList());
    for (const ClientId client : _clients) {
        if (!isInGame(client)) {
            _network->send(client, bytes, ecs::net::Channel::Reliable);
        }
    }
}

bool Lobby::isInGame(const ClientId client) const {
    const auto id = roomOf(client);
    if (!id) {
        return false;
    }
    const auto it = _rooms.find(*id);
    return it != _rooms.end() && it->second.isPlaying();
}

std::optional<RoomId> Lobby::roomOf(const ClientId client) const {
    const auto it = _roomOf.find(client);
    if (it == _roomOf.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace rtype::server
