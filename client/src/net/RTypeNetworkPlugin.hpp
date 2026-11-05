#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkEvents.hpp"
#include "protocol/Messages.hpp"

namespace client {

/**
 * @brief R-Type side of the client's network: the session with the server, and what replication leaves to the game.
 *
 * - Session: `Hello`, then `Welcome` opens UDP; the client joins the first room waiting for players, or creates one if
 *   there is none, and the `ready` action toggles its ready state until `GameStart`.
 * - In game: once per fixed step, sends the state of the `input::InputState` of player 0 as `ecs::net::LocalInput`.
 * - Replicated entities: the ship of this player is `ecs::Predicted`, moved by `InputApplied` like on the server; the
 *   others are `ecs::Interpolated`.
 *
 * It draws nothing and needs no window, so it runs in tests. If the server is lost, it stops the engine.
 */
class RTypeNetworkPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param transport Client transport, already connected or about to be: UDP is opened on it after `Welcome`.
     */
    explicit RTypeNetworkPlugin(std::shared_ptr<ecs::net::INetworkClient> transport);

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
     * @brief Subscribes to the network and replication events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Sends the input of this step, once the game has started.
     * @param engine Host engine.
     * @param time Time of this step.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Toggles the ready state when the `ready` action is pressed in a room.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Tells the server the client leaves, and unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Handles a reliable message; snapshots and inputs belong to the replication plugin.
     * @param engine Host engine.
     * @param packet Received packet.
     */
    void onPacket(engine::Engine& engine, const ecs::net::PacketReceived& packet);
    /**
     * @brief Joins the first room waiting for players, or creates one, if the client is in no room yet.
     *
     * Called on each `LobbyList`: if a join is refused because the room just started or filled up, the server pushes a
     * new list after that change, and the client tries again from it.
     *
     * @param engine Host engine.
     * @param list Rooms of the server.
     */
    void chooseRoom(engine::Engine& engine, const rtype::protocol::LobbyList& list) const;
    /**
     * @brief Gives `ecs::Predicted` to the ship of this player, if both the ship and `GameStart` have arrived.
     *
     * Called when either arrives: a snapshot (UDP) can overtake `GameStart` (TCP).
     *
     * @param engine Host engine.
     */
    void markLocalShip(engine::Engine& engine) const;
    /**
     * @brief Encodes and sends a message to the server, on the reliable channel.
     * @param engine Host engine.
     * @param message Message to send.
     */
    static void sendMessage(engine::Engine& engine, const rtype::protocol::Message& message);

    /** @brief Client transport, to open UDP. */
    std::shared_ptr<ecs::net::INetworkClient> _transport;
    /** @brief Room of this client, once the server has confirmed it. */
    std::optional<std::uint32_t> _room;
    /** @brief Received when the game starts: gives the index of this player's ship. */
    std::optional<rtype::protocol::GameStart> _gameStart;
    /** @brief Ready state last sent to the server. */
    bool _ready = false;
    /** @brief Sequence of the last input sent. */
    std::uint32_t _sequence = 0;
    /** @brief Subscription to `PeerConnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _connected;
    /** @brief Subscription to `PeerDisconnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _disconnected;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
    /** @brief Subscription to `InputApplied`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _inputs;
};

}  // namespace client
