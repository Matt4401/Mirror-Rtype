#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "Room.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Messages.hpp"

namespace rtype::server {

/** @brief Maximum number of players in one room. */
inline constexpr std::uint8_t MAX_PLAYERS_PER_ROOM = 4;
static_assert(MAX_PLAYERS_PER_ROOM <= protocol::MAX_PLAYERS);

/**
 * @brief Identifier of a room.
 *
 * Rooms are referred to by id, never by pointer: if a room no longer exists, looking its id up fails cleanly instead
 * of reading freed memory. A `Room&` is only taken locally, right after a lookup, and never stored.
 */
using RoomId = std::uint32_t;

/**
 * @brief The clients past their `Hello` and the rooms: clients create or join rooms, and a room starts, on its own
 * thread, when all its players are ready.
 *
 * Only used from the thread of the `ServerManager`, which owns the network and the handshake and hands it the rest.
 */
class Lobby {
  public:
    /**
     * @brief Creates an empty lobby.
     * @param network Transport shared by every room; must outlive the lobby.
     */
    explicit Lobby(ecs::net::INetworkServer& network) : _network(&network) {}

    /** @brief A client's `Hello` was accepted: it can now use the lobby. @param client The client. */
    void addClient(ClientId client);
    /** @brief A client is gone: it leaves its room and is forgotten. @param client The client. */
    void removeClient(ClientId client);
    /** @brief True if `addClient()` was called for this client. @param client The client. */
    [[nodiscard]] bool hasClient(ClientId client) const { return _clients.contains(client); }

    /**
     * @brief Handles a message of the lobby; dropped if the client's room has started.
     * @param client Sender, known to the lobby.
     * @param message Decoded message.
     */
    void onMessage(ClientId client, const protocol::Message& message);
    /**
     * @brief Hands a payload received over UDP to the room of the client, which decodes it.
     * @param client Sender.
     * @param data Untrusted payload.
     */
    void onUnreliableData(ClientId client, const ecs::net::Bytes& data);
    /**
     * @brief Removes a client from its room: destroys the room if it becomes empty, and otherwise tells the others.
     *
     * In a lobby, the remaining players may all be ready: the room then starts, or they would wait for nobody.
     *
     * @param client The client; ignored if it is in no room.
     */
    void leaveRoom(ClientId client);
    /** @brief Destroys every room and stops their threads. */
    void clear();

  private:
    /** @brief `LobbyCreate`: creates a room and puts the client in it. Dropped if the client is already in a room. */
    void onLobbyCreate(ClientId client);
    /** @brief `LobbyJoin`: dropped if the client is already in a room, or if the room is unknown, full or in game. */
    void onLobbyJoin(ClientId client, RoomId id);
    /** @brief `LobbyReady`: updates the client's room, and starts it if everyone is ready. Dropped outside a lobby. */
    void onLobbyReady(ClientId client, bool ready);

    /**
     * @brief Starts a room if all its players are ready, sends `GameStart` to each of them and the new list of rooms to
     * the other clients.
     * @return True if the room started.
     */
    bool startIfReady(RoomId id, Room& room);
    /** @brief Sends the players of a room and their ready state to each of them. */
    void sendLobbyUpdate(RoomId id, const Room& room) const;
    /** @brief The current list of rooms. */
    [[nodiscard]] protocol::LobbyList lobbyList() const;
    /** @brief Sends the list of rooms to every client in the lobby phase, after a room was created, changed or
     * destroyed. */
    void broadcastLobbyList() const;
    /** @brief True if a client's room has started: lobby messages from this client are out of phase. */
    [[nodiscard]] bool isInGame(ClientId client) const;
    /** @brief Id of a client's room, or nothing if the client is in no room. */
    [[nodiscard]] std::optional<RoomId> roomOf(ClientId client) const;

    /** @brief Transport, to reach the clients. */
    ecs::net::INetworkServer* _network;
    /**
     * @brief Rooms in progress or waiting for players, by id.
     *
     * Stored by value: an `unordered_map` never moves its elements, even when it grows, so a `Room` (neither copyable
     * nor movable) is built in place with `try_emplace`.
     */
    std::unordered_map<RoomId, Room> _rooms;
    /** @brief Room of each client in a room: routes inputs and disconnections. */
    std::unordered_map<ClientId, RoomId> _roomOf;
    /** @brief Clients whose `Hello` was accepted. */
    std::unordered_set<ClientId> _clients;
    /** @brief Id given to the next room created. */
    RoomId _nextRoomId = 1;
};

}  // namespace rtype::server
