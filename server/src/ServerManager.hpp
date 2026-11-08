#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Lobby.hpp"
#include "Room.hpp"
#include "level/Level.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Messages.hpp"

namespace rtype::server {

/**
 * @brief Time a connected client has to send its `Hello`: past it, the client is kicked.
 *
 * Without it, a peer that opens the TCP connection and stays silent would hold its slot forever (the network layer
 * only times out clients whose UDP channel is open, which happens after the handshake).
 */
inline constexpr std::chrono::milliseconds HANDSHAKE_TIMEOUT{std::chrono::seconds(5)};

/**
 * @brief Owns the network and the lobby, and runs the server loop on its thread.
 *
 * Each loop iteration polls the network once, handles the handshake, and hands the rest to the `Lobby`, which owns the
 * rooms. A started room advances on its own thread and sends its own snapshots. It holds no game logic: that lives in
 * the rooms (ECS).
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
     * @param level Level played by every room; `nullptr` plays no level.
     */
    explicit ServerManager(std::unique_ptr<ecs::net::INetworkServer> network,
                           std::chrono::milliseconds handshakeTimeout = HANDSHAKE_TIMEOUT,
                           std::shared_ptr<const level::Level> level = nullptr);
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
    /** @brief A game payload: decodes it if it is reliable, or routes it to the client's room if it is unreliable. */
    void onData(ClientId client, ecs::net::Channel channel, const ecs::net::Bytes& data);
    /** @brief A message received over TCP: handshake, lobby, chat, leave. */
    void onReliableMessage(ClientId client, const protocol::Message& message);
    /** @brief A payload received over UDP: handed to the room of the client, which decodes it. */
    void onUnreliableData(ClientId client, const ecs::net::Bytes& data);
    /** @brief Kicks the connected clients that did not send their `Hello` within the handshake timeout. */
    void expireHandshakes();
    /** @brief Encodes a message and sends it to one client. */
    void send(ClientId client, const protocol::Message& message, ecs::net::Channel channel) const;

    /** @brief Network server, polled and sent to on the game thread only. */
    std::unique_ptr<ecs::net::INetworkServer> _network;
    /** @brief Port the server listens on (TCP and UDP), sent to clients in `Welcome`. */
    std::uint16_t _port = 0;
    /** @brief Clients past their `Hello` and rooms. Declared after `_network`, which it sends through. */
    Lobby _lobby;
    /** @brief Connection time of each connected client that has not completed its handshake yet. */
    std::unordered_map<ClientId, std::chrono::steady_clock::time_point> _pendingHandshakes;
    /** @brief Time a connected client has to send its `Hello`. */
    std::chrono::milliseconds _handshakeTimeout;
    /** @brief Loop flag, cleared by `stop()`. */
    std::atomic<bool> _running{false};
};

}  // namespace rtype::server
