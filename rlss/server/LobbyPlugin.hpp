/**
 * @file LobbyPlugin.hpp
 * @brief Server plugin tracking the connected players: one car each, and the player count sent to every client.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"

namespace rlss::server {

/**
 * @brief Gives each connected peer a slot (its color and spawn point) and a car, and broadcasts a `LobbyState` each
 * time a player joins or leaves.
 *
 * Listens to the `PeerConnected` / `PeerDisconnected` events published by `ecs::net::NetworkPlugin`, and sends
 * through the `ecs::net::INetworkService` it registers. A car is an entity driven by the `Car` Wren script, owned by
 * its peer (`ecs::NetworkOwner`) and replicated to every client. This is where the match logic will grow.
 */
class LobbyPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "LobbyPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "LobbyPlugin"; }

    /**
     * @brief Declares the dependencies on `ecs::net::NetworkPlugin` and `CarSimulationPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the network events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the network events and removes the cars.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Number of clients currently connected.
     * @return Player count.
     */
    [[nodiscard]] std::size_t playerCount() const noexcept { return _players.size(); }

  private:
    /** @brief What the server keeps about a connected player. */
    struct Player {
        /** @brief Slot: color index and spawn point. */
        std::size_t slot = 0;
        /** @brief Car entity. */
        ecs::Entity car = 0;
    };

    /**
     * @brief Gives a new peer the lowest free slot and spawns its car.
     * @param engine Host engine.
     * @param peer New peer.
     */
    void join(engine::Engine& engine, ecs::net::PeerId peer);

    /**
     * @brief Removes a peer's car (through the script system, which runs its `onDestroy`) and frees its slot.
     * @param engine Host engine.
     * @param peer Peer that left.
     * @return False if the peer was not a player.
     */
    bool leave(engine::Engine& engine, ecs::net::PeerId peer);

    /**
     * @brief Sends the current player count to every client.
     * @param engine Host engine.
     */
    void broadcastState(engine::Engine& engine) const;

    /** @brief Connected players. */
    std::unordered_map<ecs::net::PeerId, Player> _players;
    /** @brief Slots in use, by slot index. */
    std::vector<bool> _slots;
    /** @brief Subscription to `PeerConnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _connected;
    /** @brief Subscription to `PeerDisconnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _disconnected;
};

}  // namespace rlss::server
