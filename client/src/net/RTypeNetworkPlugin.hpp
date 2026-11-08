/**
 * @file RTypeNetworkPlugin.hpp
 * @brief The client's session with the server, driven by request events and reporting result events.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "net/ClientConnection.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkEvents.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"

namespace client {

/** @brief Where the session with the server is. */
enum class SessionState : std::uint8_t {
    /** @brief No connection. */
    Offline,
    /** @brief Connecting, or waiting for `Welcome`. */
    Connecting,
    /** @brief In the lobby, in no room. */
    Lobby,
    /** @brief Waiting in a room. */
    InRoom,
    /** @brief Playing. */
    InGame,
};

/**
 * @brief The session with the server: connection, lobby, rooms, game.
 *
 * The game drives it with the request events of `SessionEvents.hpp` (`ConnectRequest`, `JoinRoomRequest`...), and it
 * reports what happens with the result events (`SessionConnected`, `RoomUpdated`, `GameStarted`, `LevelStarted`,
 * `GameEnded`, `SessionClosed`...). A request that makes no sense in the current state is ignored. The last state
 * received (list of rooms, room, level, result) is also kept, for a screen opened after the event.
 *
 * - **Connecting:** each `ConnectRequest` uses a fresh transport from the factory (a transport connects once), put in
 *   the shared `ClientConnection`, then `Hello` and `Welcome`.
 * - **Game:** `GameStart` opens UDP. `GameOver`, or leaving the game, closes it and resets the client replication, so
 *   the next game starts from a clean state.
 * - **Failures:** a refused, failed or lost connection publishes `SessionClosed` with a reason for the player. It never
 *   stops the engine: the game goes back to its menus.
 *
 * It draws nothing and needs no window, so it runs in tests.
 */
class RTypeNetworkPlugin final : public engine::IPlugin {
  public:
    /** @brief Builds a new transport for each connection attempt. */
    using TransportFactory = std::function<std::shared_ptr<ecs::net::INetworkClient>()>;

    /**
     * @brief Creates the plugin.
     * @param connection Connection given to `ecs::net::NetworkPlugin`; its transport is replaced at each connection.
     * @param factory Builds the transports.
     */
    RTypeNetworkPlugin(std::shared_ptr<ecs::net::ClientConnection> connection, TransportFactory factory);

    /**
     * @brief Gets plugin name.
     * @return String view "RTypeNetworkPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeNetworkPlugin"; }

    /**
     * @brief Declares the dependencies on the network plugins.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the network events and to the requests.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Drops the transport of a connection that ended.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Tells the server the client leaves, and unsubscribes.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Gets where the session is.
     * @return Current state.
     */
    [[nodiscard]] SessionState state() const noexcept { return _state; }

    /**
     * @brief Gets the last list of rooms received.
     * @return The list, empty before the first one.
     */
    [[nodiscard]] const rtype::protocol::LobbyList& rooms() const noexcept { return _rooms; }

    /**
     * @brief Gets the room the client is in.
     * @return The last update of the room, or nothing outside a room.
     */
    [[nodiscard]] const std::optional<rtype::protocol::LobbyUpdate>& room() const noexcept { return _room; }

    /**
     * @brief Gets the ship of this client in the game.
     * @return Its player index, or nothing outside a game.
     */
    [[nodiscard]] std::optional<std::uint8_t> playerIndex() const noexcept { return _playerIndex; }

    /**
     * @brief Gets the level being played.
     * @return The last `LevelStart` of this game, or nothing.
     */
    [[nodiscard]] const std::optional<rtype::protocol::LevelStart>& level() const noexcept { return _level; }

    /**
     * @brief Gets the result of the last game.
     * @return Its `GameOver`, or nothing.
     */
    [[nodiscard]] const std::optional<rtype::protocol::GameOver>& lastResult() const noexcept { return _result; }

    /**
     * @brief Gets the reason the last session ended without being asked to.
     * @return Message for the player, or empty.
     */
    [[nodiscard]] const std::string& lastError() const noexcept { return _lastError; }

  private:
    /**
     * @brief Subscribes to an event until shutdown.
     * @tparam Event Event type.
     * @param engine Host engine.
     * @param handler Handler.
     */
    template <typename Event, typename Handler>
    void listen(engine::Engine& engine, Handler handler);

    /**
     * @brief Starts a connection.
     * @param engine Host engine.
     * @param request Where to connect.
     */
    void onConnect(engine::Engine& engine, const ConnectRequest& request);
    /**
     * @brief The connection could not be made, or is gone: publishes `SessionClosed` and goes offline.
     * @param engine Host engine.
     */
    void onDisconnected(engine::Engine& engine);
    /**
     * @brief Handles a reliable message; snapshots and inputs belong to the replication plugin.
     * @param engine Host engine.
     * @param packet Received packet.
     */
    void onPacket(engine::Engine& engine, const ecs::net::PacketReceived& packet);
    /**
     * @brief Handles a decoded message of the server.
     * @param engine Host engine.
     * @param message The message.
     */
    void onMessage(engine::Engine& engine, const rtype::protocol::Message& message);
    /**
     * @brief The game starts: opens UDP and publishes `GameStarted`, or closes the session if UDP cannot be opened.
     * @param engine Host engine.
     * @param start The message of the server.
     */
    void onGameStart(engine::Engine& engine, const rtype::protocol::GameStart& start);
    /**
     * @brief Leaves the game locally: closes UDP and resets the replication.
     * @param engine Host engine.
     */
    void leaveGame(engine::Engine& engine);
    /**
     * @brief Closes the session.
     * @param engine Host engine.
     * @param reason Reason for the player; empty when the game asked for it (no `SessionClosed` then).
     * @param duringConnection Whether the handshake was still in progress.
     */
    void close(engine::Engine& engine, const std::string& reason, bool duringConnection);
    /**
     * @brief Encodes and sends a message to the server, on the reliable channel.
     * @param engine Host engine.
     * @param message Message to send.
     */
    static void sendMessage(engine::Engine& engine, const rtype::protocol::Message& message);

    /** @brief Connection shared with `NetworkPlugin`. */
    std::shared_ptr<ecs::net::ClientConnection> _connection;
    /** @brief Builds the transports. */
    TransportFactory _factory;
    /** @brief Transport of a session that ended, dropped at the next update (not inside its own event dispatch). */
    std::shared_ptr<ecs::net::INetworkClient> _retired;
    /** @brief Where the session is. */
    SessionState _state = SessionState::Offline;
    /** @brief UDP port of the server, received in `Welcome`. */
    std::uint16_t _udpPort = 0;
    /** @brief Last list of rooms. */
    rtype::protocol::LobbyList _rooms;
    /** @brief Room of this client. */
    std::optional<rtype::protocol::LobbyUpdate> _room;
    /** @brief Ship of this client during a game. */
    std::optional<std::uint8_t> _playerIndex;
    /** @brief Level being played. */
    std::optional<rtype::protocol::LevelStart> _level;
    /** @brief Result of the last game. */
    std::optional<rtype::protocol::GameOver> _result;
    /** @brief Reason the last session ended. */
    std::string _lastError;
    /** @brief Cancels the subscriptions. */
    std::vector<std::function<void()>> _unsubscribe;
};

}  // namespace client
