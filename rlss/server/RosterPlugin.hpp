/**
 * @file RosterPlugin.hpp
 * @brief Server plugin tracking the players of one match: their team, their slot and their car.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::server {

/**
 * @brief Gives each player of the match a team, a slot (its color and kickoff spot) and a car.
 *
 * The match's peers come and go through the `PeerConnected` / `PeerDisconnected` events published by
 * `ecs::net::NetworkPlugin` (the `MatchServer` pushes them when a player joins or leaves the match). At most
 * `protocol::MAX_PLAYERS_PER_MATCH` players, `protocol::PLAYERS_PER_TEAM` per team: a newcomer goes to the team with
 * fewer players (blue on a tie). Even slots are blue, odd slots orange, as `Kickoff.wren` expects.
 *
 * A car is an entity driven by the `Car` Wren script, owned by its peer (`ecs::NetworkOwner`) and replicated to every
 * player of the match; the peer is told its network id and team (`Welcome`).
 */
class RosterPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "RosterPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "RosterPlugin"; }

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
     * @brief Unsubscribes from the network events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Number of players in a team.
     * @param team Team.
     * @return Player count, at most `protocol::PLAYERS_PER_TEAM`.
     */
    [[nodiscard]] std::uint8_t playersIn(protocol::Team team) const noexcept;

    /**
     * @brief Number of players in the match.
     * @return Player count.
     */
    [[nodiscard]] std::size_t playerCount() const noexcept { return _players.size(); }

  private:
    /** @brief What the server keeps about a player. */
    struct Player {
        /** @brief Slot: color index, team and kickoff spot. */
        std::size_t slot = 0;
        /** @brief Car entity. */
        ecs::Entity car = 0;
    };

    /**
     * @brief Gives a new peer a slot in the team with fewer players and spawns its car.
     * @param engine Host engine.
     * @param peer New peer.
     * @return False if the peer already plays or the match is full.
     */
    bool join(engine::Engine& engine, ecs::net::PeerId peer);

    /**
     * @brief Removes a peer's car (through the script system, which runs its `onDestroy`) and frees its slot.
     * @param engine Host engine.
     * @param peer Peer that left.
     * @return False if the peer was not a player.
     */
    bool leave(engine::Engine& engine, ecs::net::PeerId peer);

    /**
     * @brief Free slot for a newcomer: the lowest one of the team with fewer players.
     * @return The slot, or nothing if the match is full.
     */
    [[nodiscard]] std::optional<std::size_t> freeSlot() const;

    /** @brief Players, by peer. */
    std::unordered_map<ecs::net::PeerId, Player> _players;
    /** @brief Slots in use. */
    std::array<bool, protocol::MAX_PLAYERS_PER_MATCH> _slots{};
    /** @brief Subscription to `PeerConnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _connected;
    /** @brief Subscription to `PeerDisconnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _disconnected;
};

}  // namespace rlss::server
