/**
 * @file LobbyPlugin.hpp
 * @brief Server plugin tracking the connected players and telling every client how many there are.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"

namespace rlss::server {

/**
 * @brief Keeps the set of connected peers and broadcasts a `LobbyState` each time it changes.
 *
 * Listens to the `PeerConnected` / `PeerDisconnected` events published by `ecs::net::NetworkPlugin`, and sends
 * through the `ecs::net::INetworkService` it registers. This is where the match logic will grow.
 */
class LobbyPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "LobbyPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "LobbyPlugin"; }

    /**
     * @brief Declares the dependency on `ecs::net::NetworkPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the network events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the network events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Number of clients currently connected.
     * @return Player count.
     */
    [[nodiscard]] std::size_t playerCount() const noexcept { return _players.size(); }

  private:
    /**
     * @brief Sends the current player count to every client.
     * @param engine Host engine.
     */
    void broadcastState(engine::Engine& engine) const;

    /** @brief Connected peers. */
    std::unordered_set<ecs::net::PeerId> _players;
    /** @brief Subscription to `PeerConnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _connected;
    /** @brief Subscription to `PeerDisconnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _disconnected;
};

}  // namespace rlss::server
