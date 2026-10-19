#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"
#include "Room.hpp"

namespace rtype::server {

/** @brief Maximum number of players in one room. */
inline constexpr std::uint8_t MAX_PLAYERS_PER_ROOM = 4;

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
     * The network is injected: `main()` passes `net::createServer()`, a test can pass a mock.
     *
     * @param network Network server, owned by the ServerManager.
     */
    explicit ServerManager(std::unique_ptr<net::server::INetworkServer> network);
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
    /** @brief A game message: lobby messages over TCP, inputs over UDP. */
    void onData(ClientId client, net::Channel channel, const net::Bytes& data) const;

    /** @brief Lets each room run the steps due on its own clock, and sends a snapshot for each room that stepped. */
    void advanceRooms();
    /** @brief Sends the room's latest state to each of its players. */
    void sendSnapshot(const Room& room) const;

    /**
     * @brief Puts a client in an open room, or in a new one.
     * @return Id of the client's room.
     */
    RoomId joinRoom(ClientId client);
    /** @brief Removes a client from its room, and destroys the room if it becomes empty. */
    void leaveRoom(ClientId client);
    /** @brief Id of a client's room, or nothing if the client is in the lobby. */
    [[nodiscard]] std::optional<RoomId> roomOf(ClientId client) const;

    /** @brief Network server, polled and sent to on the game thread only. */
    std::unique_ptr<net::server::INetworkServer> _network;
    /**
     * @brief Rooms in progress or waiting for players, by id.
     *
     * Stored by value: an `unordered_map` never moves its elements, even when it grows, so a `Room` (neither copyable
     * nor movable) is built in place with `try_emplace`.
     */
    std::unordered_map<RoomId, Room> _rooms;
    /** @brief Room of each client in a room: routes inputs and disconnections. */
    std::unordered_map<ClientId, RoomId> _roomOf;
    /** @brief Id given to the next room created. */
    RoomId _nextRoomId = 1;
    /** @brief Loop flag, cleared by `stop()`. */
    std::atomic<bool> _running{false};
};

}  // namespace rtype::server
