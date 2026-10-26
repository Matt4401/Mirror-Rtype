#include "ServerManager.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"
#include "Room.hpp"

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
    log::info(LOG_TAG, "client ", client, " disconnected");
}

void ServerManager::onData(const ClientId client, const net::Channel channel,
                           [[maybe_unused]] const net::Bytes& data) const {
    log::debug(LOG_TAG, "client ", client, " sent ", data.size(), " bytes over ",
               channel == net::Channel::Reliable ? "TCP" : "UDP");
    if (channel == net::Channel::Reliable) {
        // TODO(protocol): decode the lobby message (join, ready, chat, leave...), e.g.:
        // JOIN  -> joinRoom(client)
        // READY -> when every player of the room is ready: room.start(), then GAME_START to each player
        // (onData then modifies the rooms: remove its const)
        return;
    }

    if (!roomOf(client)) {
        log::debug(LOG_TAG, "client ", client, " is not in a room: UDP data ignored");
        return;
    }
    // TODO(protocol): if (const auto room = _rooms.find(*roomOf(client)); room != _rooms.end()) {
    //                     room->second.onInput(client, protocol::decodeInput(data));
    //                 }
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

// Not static on purpose: sending the snapshot will use _network once the protocol exists.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void ServerManager::sendSnapshot(const Room& room) const {
    for (const ClientId player : room.players()) {
        // TODO(protocol): encode room.time().tickCount (orders snapshots on the client),
        // room.lastProcessedInput(player) and the entities of room.registry(), then:
        // _network->send(player, snapshot, net::Channel::Unreliable);
        static_cast<void>(player);
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
    for (const ClientId other : room->second.players()) {
        // TODO(protocol): tell the remaining players that `client` left (check the exact wording in the subject, p.10),
        // on the reliable channel: _network->send(other, protocol::encodePlayerLeft(client), net::Channel::Reliable);
        static_cast<void>(other);
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
