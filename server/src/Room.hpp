#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "components/InputBuffer.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/PeerGroupConnection.hpp"

namespace rtype::server {

/** @brief Maximum number of inputs queued per player; when exceeded, the oldest one is dropped. */
inline constexpr std::size_t MAX_PENDING_INPUTS = ecs::DEFAULT_MAX_PENDING_INPUTS;

/** @brief Network id of a player: the transport's peer id. */
using ClientId = ecs::net::PeerId;

/** @brief One player of a room. */
struct RoomPlayer {
    /** @brief Network id of the player. */
    ClientId client = 0;
    /** @brief Ship of the player; its network state is in its components. */
    ecs::Entity entity = 0;
    /** @brief Ready state in the lobby. */
    bool ready = false;
};

/**
 * @brief One game: its own `engine::Engine` and its players.
 *
 * Each room runs its own engine (ECS world, event bus, clock, plugins), so games are isolated from each other: the
 * systems of a room never see the entities of another one, and its time starts at 0 when the game starts.
 *
 * The network goes through `ecs::net::NetworkPlugin` and `ecs::net::ServerReplicationPlugin`: the room receives raw
 * payloads from the `ServerManager`, the plugin decodes the inputs, and sends the snapshots to the players of this room
 * only. A player's entity carries `ecs::NetworkOwner`, `ecs::NetworkId` and `ecs::InputBuffer`.
 *
 * The `ServerManager` calls `advance()` on every room in the same loop (one thread), but each room decides by itself
 * how many steps are due.
 */
class Room {
  public:
    /**
     * @brief Creates an empty room, waiting for players.
     * @param network Transport shared by every room; must outlive the room.
     * @param maxPlayers Maximum number of players in this room.
     * @param fixedDt Duration of one simulation step in seconds (60 Hz by default).
     * @param timeSource Time source of the room's clock; `nullptr` for the real time. Injectable for tests.
     */
    Room(ecs::net::INetworkServer& network, std::uint8_t maxPlayers, float fixedDt = ecs::time::DEFAULT_FIXED_DT,
         ecs::time::Clock::TimeSource timeSource = nullptr);

    /**
     * @brief Adds a player and spawns its ship.
     * @param player Network id of the player.
     * @return False if the room is full, already playing, or the player is already in it.
     */
    bool addPlayer(ClientId player);

    /**
     * @brief Removes a player and kills its entity. Does nothing if the player is not in the room.
     * @param player Network id of the player.
     */
    void removePlayer(ClientId player);

    /**
     * @brief Hands over a payload a player sent on the unreliable channel; it is decoded during the next `advance()`.
     *
     * Dropped while the game has not started: nothing would consume it, and a client must not be able to fill the
     * queue. Payloads that are not valid inputs are dropped by the codec. One input per player is applied at each
     * simulation step, oldest first; see `ecs::net::pushInput()` for late, duplicated and excess inputs.
     *
     * @param player Network id of the sender.
     * @param data Untrusted payload.
     */
    void onPacket(ClientId player, const ecs::net::Bytes& data);

    /**
     * @brief Sets the ready state of a player. Does nothing once the game has started, or if the player is not here.
     * @param player Network id of the player.
     * @param ready New ready state.
     */
    void setReady(ClientId player, bool ready);

    /** @brief True when the room has at least one player and all of them are ready. */
    [[nodiscard]] bool isEveryoneReady() const;

    /**
     * @brief Starts the game: no player can join anymore, and `advance()` runs the simulation.
     *
     * Each player's index becomes its position in `players()`, without holes whoever left the lobby, and its ship goes
     * to its spawn point. Resets the room's clock, so that the game time (`tickCount`, `totalElapsedTime`) starts at 0
     * now, whatever how long the room waited for its players.
     */
    void start();

    /**
     * @brief Runs the simulation steps that are due according to the room's clock, then sends a snapshot if any ran.
     *
     * Does nothing while the room is waiting for players.
     *
     * @return Number of steps executed (0 if none was due).
     */
    std::size_t advance();

    /** @brief True once `start()` has been called. */
    [[nodiscard]] bool isPlaying() const { return _playing; }
    /** @brief True when no other player can join. */
    [[nodiscard]] bool isFull() const { return _players.size() >= _maxPlayers; }
    /** @brief True when the last player has left: the `ServerManager` then destroys the room. */
    [[nodiscard]] bool isEmpty() const { return _players.empty(); }
    /**
     * @brief Players of this room, in join order.
     *
     * The span is invalidated if a player is added or removed while iterating over it.
     */
    [[nodiscard]] std::span<const RoomPlayer> players() const { return _players; }

    /**
     * @brief Sequence number of the last input applied for a player, sent in snapshots for client reconciliation.
     * @param player Network id of the player.
     * @return 0 if no input was applied yet, or if the player is not in the room.
     */
    [[nodiscard]] std::uint32_t lastProcessedInput(ClientId player) const;

    /** @brief ECS world of the room. */
    [[nodiscard]] ecs::Registry& registry() { return _engine.registry(); }
    /** @brief ECS world of the room, read-only. */
    [[nodiscard]] const ecs::Registry& registry() const { return _engine.registry(); }

  private:
    /** @brief Maximum number of players. */
    std::uint8_t _maxPlayers;
    /** @brief True once the game has started. */
    bool _playing = false;
    /** @brief Network of this room: what its engine receives and the only peers it can reach. */
    std::shared_ptr<engine::PeerGroupConnection> _connection;
    /** @brief Engine of this game: ECS world, event bus, clock and plugins. */
    engine::Engine _engine;
    /** @brief Players of the room, in join order: at most `_maxPlayers`, so looking one up is a short scan. */
    std::vector<RoomPlayer> _players;
};

}  // namespace rtype::server
