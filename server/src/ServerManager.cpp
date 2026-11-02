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
#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"
#include "Room.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace rtype::server {

namespace {

// Sleep between two loop iterations. Short enough to keep the step jitter around 1 ms, long enough not to burn a core.
constexpr auto IDLE_SLEEP = std::chrono::milliseconds(1);

}  // namespace

ServerManager::ServerManager(std::unique_ptr<net::server::INetworkServer> network) : _network(std::move(network)) {}

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
        advanceRooms();
        std::this_thread::sleep_for(IDLE_SLEEP);
    }

    _network->stop();
    log::info(LOG_TAG, "stopped");
    return true;
}

void ServerManager::stop() { _running = false; }

void ServerManager::handleEvents() {
    for (const auto& [type, client, data, channel] : _network->poll()) {
        switch (type) {
            case net::EventType::Connected:
                onConnected(client);
                break;
            case net::EventType::Disconnected:
                onDisconnected(client);
                break;
            case net::EventType::Data:
                onData(client, channel, data);
                break;
        }
    }
}

// Not static on purpose: the lobby logic (e.g. sending the list of rooms) will need the members.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ServerManager::onConnected(const ClientId client) {
    // The client is in the lobby: it joins a room through a lobby message (see onData).
    log::info(LOG_TAG, "client ", client, " connected");
}

void ServerManager::onDisconnected(const ClientId client) {
    // Can arrive without onConnected (connection lost before being ready): leaveRoom() then does nothing.
    leaveRoom(client);
    _handshaken.erase(client);
    log::info(LOG_TAG, "client ", client, " disconnected");
}

void ServerManager::onData(const ClientId client, const net::Channel channel, const net::Bytes& data) {
    const auto message = protocol::decode(data);
    if (!message) {
        log::debug(LOG_TAG, "client ", client, " sent an invalid payload of ", data.size(), " bytes: dropped");
        return;
    }
    if (channel == net::Channel::Reliable) {
        onReliableMessage(client, *message);
    } else {
        onUnreliableMessage(client, *message);
    }
}

void ServerManager::onReliableMessage(const ClientId client, const protocol::Message& message) {
    if (const auto* hello = std::get_if<protocol::Hello>(&message)) {
        if (_handshaken.contains(client)) {
            log::debug(LOG_TAG, "client ", client, " sent a second Hello: dropped");
            return;
        }
        if (hello->version != protocol::PROTOCOL_VERSION) {
            log::info(LOG_TAG, "client ", client, " uses protocol version ", hello->version, " (expected ",
                      protocol::PROTOCOL_VERSION, "): kicked");
            _network->kick(client);
            return;
        }
        _handshaken.insert(client);
        send(client, protocol::Welcome{.udpPort = _port}, net::Channel::Reliable);
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
    // TODO(protocol): lobby messages, e.g.:
    // LobbyListRequest -> LobbyList
    // LobbyCreate / LobbyJoin -> joinRoom(client), then LobbyUpdate to the room
    // LobbyReady -> when every player of the room is ready: room.start(), then GameStart to each player
    // Chat -> fill clientId, then send to the other players of the room
    log::debug(LOG_TAG, "client ", client, " sent an unhandled message (index ", message.index(), ")");
}

void ServerManager::onUnreliableMessage(const ClientId client, const protocol::Message& message) {
    if (!_handshaken.contains(client)) {
        log::debug(LOG_TAG, "client ", client, " sent UDP data before Hello: dropped");
        return;
    }
    const auto* input = std::get_if<protocol::PlayerInput>(&message);
    if (input == nullptr) {
        log::debug(LOG_TAG, "client ", client, " sent a non-input message over UDP: dropped");
        return;
    }
    const auto id = roomOf(client);
    if (!id) {
        log::debug(LOG_TAG, "client ", client, " is not in a room: input ignored");
        return;
    }
    if (const auto room = _rooms.find(*id); room != _rooms.end()) {
        room->second.onInput(client, PlayerInput{.sequence = input->sequence, .actions = input->buttons});
    }
}

void ServerManager::send(const ClientId client, const protocol::Message& message, const net::Channel channel) const {
    _network->send(client, protocol::encode(message), channel);
}

void ServerManager::advanceRooms() {
    for (auto& room : _rooms | std::views::values) {
        // Each room runs the steps due on its own clock. After several catch-up steps, one snapshot is enough: it
        // carries the latest state.
        if (room.advance() > 0) {
            sendSnapshot(room);
        }
    }
}

void ServerManager::sendSnapshot(const Room& room) const {
    protocol::WorldState snapshot{.tick = static_cast<std::uint32_t>(room.time().tickCount)};
    // TODO(protocol): fill snapshot.entities from room.registry() once the entities carry a type component.
    for (const ClientId player : room.players()) {
        snapshot.lastProcessedInput = room.lastProcessedInput(player);
        const auto bytes = protocol::encode(snapshot);
        if (bytes.size() > protocol::MAX_UNRELIABLE_SIZE) {
            log::error(LOG_TAG, "snapshot of ", bytes.size(), " bytes does not fit in a datagram: not sent");
            return;
        }
        _network->send(player, bytes, net::Channel::Unreliable);
    }
}

std::optional<RoomId> ServerManager::joinRoom(const ClientId client) {
    if (const auto current = roomOf(client)) {
        return current;
    }

    std::optional<RoomId> open;
    for (const auto& [id, room] : _rooms) {
        if (!room.isPlaying() && !room.isFull()) {
            open = id;
            break;
        }
    }
    const RoomId id = open.value_or(_nextRoomId);
    if (!open) {
        _rooms.try_emplace(_nextRoomId++, MAX_PLAYERS_PER_ROOM);
        log::debug(LOG_TAG, "room ", id, " created");
    }

    Room& room = _rooms.at(id);
    if (!room.addPlayer(client)) {
        return std::nullopt;
    }
    _roomOf[client] = id;
    log::debug(LOG_TAG, "client ", client, " joined room ", id, " (", room.players().size(), '/',
               static_cast<int>(MAX_PLAYERS_PER_ROOM), " players)");
    return id;
}

void ServerManager::leaveRoom(const ClientId client) {
    const auto id = roomOf(client);
    if (!id) {
        return;
    }
    _roomOf.erase(client);

    const auto room = _rooms.find(*id);
    if (room == _rooms.end()) {
        return;
    }
    room->second.removePlayer(client);
    log::debug(LOG_TAG, "client ", client, " left room ", *id);
    const auto left = protocol::encode(protocol::PlayerLeft{.clientId = client});
    for (const ClientId other : room->second.players()) {
        _network->send(other, left, net::Channel::Reliable);
    }
    if (room->second.isEmpty()) {
        _rooms.erase(room);
        log::debug(LOG_TAG, "room ", *id, " destroyed (empty)");
    }
}

std::optional<RoomId> ServerManager::roomOf(const ClientId client) const {
    const auto it = _roomOf.find(client);
    if (it == _roomOf.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace rtype::server
