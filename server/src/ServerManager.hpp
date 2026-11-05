#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
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

/**
 * @brief Time a connected client has to send its `Hello`: past it, the client is kicked.
 *
 * Without it, a peer that opens the TCP connection and stays silent would hold its slot forever (the network layer
 * only times out clients whose UDP channel is open, which happens after the handshake).
 */
inline constexpr std::chrono::milliseconds HANDSHAKE_TIMEOUT{std::chrono::seconds(5)};

/**
 * @brief Identifier of a room.
 *
 * Rooms are referred to by id, never by pointer: if a room no longer exists, looking its id up fails cleanly instead
 * of reading freed memory. A `Room&` is only taken locally, right after a lookup, and never stored.
 */
using RoomId = std::uint32_t;

/**
 * @brief Owns the network and the rooms, and runs the server loop on the game thread.
 *
 * Each loop iteration: poll the network once and route the events to the rooms, then let each room advance on its own
 * clock, and send a snapshot for each room that ran at least one step. It holds no game logic: that lives in the rooms
 * (ECS).
 */
class ServerManager {
  public:
    /**
     * @brief Creates the server around a network implementation.
     *
     * The network is injected: `main()` passes `ecs::net::createServer()`, a test can pass a mock.
     *
     * @param network Network server, owned by the ServerManager.
     * @param handshakeTimeout Time a connected client has to send its `Hello` before being kicked.
     */
    explicit ServerManager(std::unique_ptr<ecs::net::INetworkServer> network,
                           std::chrono::milliseconds handshakeTimeout = HANDSHAKE_TIMEOUT);
    /** @brief Default destructor. */
    ~ServerManager() = default;

    /** @brief Deleted copy constructor. */
    ServerManager(const ServerManager&) = delete;
    /** @brief Deleted copy assignment operator. */
    ServerManager& operator=(const ServerManager&) = delete;
    /** @brief Deleted move constructor. */
    ServerManager(ServerManager&&) = delete;
    /** @brief Deleted move assignment operator. */
    ServerManager& operator=(ServerManager&&) = delete;

    /**
     * @brief Starts the network and runs the server loop until `stop()`.
     * @param port TCP and UDP port to listen on.
     * @return False if the network cannot start (port taken), true once stopped.
     */
    bool run(std::uint16_t port);

    /** @brief Asks `run()` to return. Thread-safe and signal-safe: it only sets an atomic flag. */
    void stop();

  private:
    /**
     * @brief Polls the network and dispatches each event.
     *
     * The only place that reads the network: it has a single event queue for every player, so a room polling it
     * would steal the events of the others.
     */
    void handleEvents();
    /** @brief A client is connected and is in the lobby. */
    void onConnected(ClientId client);
    /** @brief A client is gone. Can arrive without `onConnected()` (connection lost before being ready). */
    void onDisconnected(ClientId client);
    /** @brief A game message: decodes it and routes it; invalid payloads are dropped. */
    void onData(ClientId client, ecs::net::Channel channel, const ecs::net::Bytes& data);
    /** @brief A message received over TCP: handshake, lobby, chat, leave. */
    void onReliableMessage(ClientId client, const protocol::Message& message);
    /** @brief A message received over UDP: only inputs, from a client in a room. */
    void onUnreliableMessage(ClientId client, const protocol::Message& message);
    /** @brief Kicks the connected clients that did not send their `Hello` within the handshake timeout. */
    void expireHandshakes();
    /** @brief Encodes a message and sends it to one client. */
    void send(ClientId client, const protocol::Message& message, ecs::net::Channel channel) const;

    /** @brief Lets each room run the steps due on its own clock, and sends a snapshot for each room that stepped. */
    void advanceRooms();
    /** @brief Sends the room's latest state to each of its players. */
    void sendSnapshot(const Room& room) const;

    /**
     * @brief Puts a client in an open room, or in a new one.
     * @return Id of the client's room.
     */
    std::optional<RoomId> joinRoom(ClientId client);
    /** @brief Removes a client from its room, and destroys the room if it becomes empty. */
    void leaveRoom(ClientId client);
    /** @brief Id of a client's room, or nothing if the client is in the lobby. */
    [[nodiscard]] std::optional<RoomId> roomOf(ClientId client) const;

    /** @brief Network server, polled and sent to on the game thread only. */
    std::unique_ptr<ecs::net::INetworkServer> _network;
    /** @brief Port the server listens on (TCP and UDP), sent to clients in `Welcome`. */
    std::uint16_t _port = 0;
    /**
     * @brief Rooms in progress or waiting for players, by id.
     *
     * Stored by value: an `unordered_map` never moves its elements, even when it grows, so a `Room` (neither copyable
     * nor movable) is built in place with `try_emplace`.
     */
    std::unordered_map<RoomId, Room> _rooms;
    /** @brief Room of each client in a room: routes inputs and disconnections. */
    std::unordered_map<ClientId, RoomId> _roomOf;
    /** @brief Clients whose `Hello` was accepted; any other message from a client not in this set is dropped. */
    std::unordered_set<ClientId> _handshaken;
    /** @brief Connection time of each connected client that has not completed its handshake yet. */
    std::unordered_map<ClientId, std::chrono::steady_clock::time_point> _pendingHandshakes;
    /** @brief Time a connected client has to send its `Hello`. */
    std::chrono::milliseconds _handshakeTimeout;
    /** @brief Id given to the next room created. */
    RoomId _nextRoomId = 1;
    /** @brief Loop flag, cleared by `stop()`. */
    std::atomic<bool> _running{false};
};

}  // namespace rtype::server
