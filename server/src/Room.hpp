#pragma once

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <unordered_map>

#include "components/InputBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkEntityMap.hpp"

namespace rtype::server {

/** @brief Maximum number of inputs queued per player; when exceeded, the oldest one is dropped. */
inline constexpr std::size_t MAX_PENDING_INPUTS = ecs::DEFAULT_MAX_PENDING_INPUTS;

/** @brief Network id of a player: the transport's peer id. */
using ClientId = ecs::net::PeerId;

/** @brief Value of one axis action during an input: the engine's generic network axis. */
using AxisInput = ecs::NetAxis;

/**
 * @brief Decoded input of one player.
 *
 * Converted from `protocol::PlayerInput` by the `ServerManager`, so that the room does not depend on the protocol.
 * It is the engine's generic network input, stored in the `ecs::InputBuffer` of the player's entity.
 */
using PlayerInput = ecs::NetInput;

/**
 * @brief One game: its own ECS world and its players.
 *
 * Each room owns its `ecs::Registry` and `ecs::EventBus`, so games are isolated from each other: the systems of a
 * room never see the entities of another one.
 *
 * The network state of a player lives in the ECS: its entity carries `ecs::NetworkOwner`, `ecs::NetworkId` and
 * `ecs::InputBuffer`, and the `ecs::net::NetworkEntityMap` resource maps network ids to entities.
 *
 * A room knows neither the network nor the protocol: it receives decoded inputs and exposes its state, and the
 * `ServerManager` encodes and sends it. It can be tested without any socket.
 *
 * Each room also owns its `ecs::time::Clock`: its time starts at 0 when the game starts, and it runs at its own rate.
 * The `ServerManager` calls `advance()` on every room in the same loop (one thread), but each room decides by itself
 * how many steps are due.
 */
class Room {
  public:
    /**
     * @brief Creates an empty room, waiting for players.
     * @param maxPlayers Maximum number of players in this room.
     * @param fixedDt Duration of one simulation step in seconds (60 Hz by default).
     * @param timeSource Time source of the room's clock; `nullptr` for the real time. Injectable for tests.
     */
    explicit Room(std::uint8_t maxPlayers, float fixedDt = ecs::time::DEFAULT_FIXED_DT,
                  ecs::time::Clock::TimeSource timeSource = nullptr);

    /**
     * @brief Adds a player and spawns its entity.
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
     * @brief Queues an input; one input per player is applied at each simulation step, oldest first.
     *
     * Inputs come over UDP: they can arrive late, duplicated or out of order. An input already applied
     * (sequence <= last processed) or already queued is ignored; the others are queued sorted by sequence. At most
     * `MAX_PENDING_INPUTS` are kept: when the queue overflows, the oldest input is dropped.
     *
     * @param player Network id of the player.
     * @param input Decoded input.
     */
    void onInput(ClientId player, const PlayerInput& input);

    /**
     * @brief Starts the game: no player can join anymore, and `advance()` runs the simulation.
     *
     * Resets the room's clock, so that the game time (`time().tickCount`, `time().totalElapsedTime`) starts at 0
     * now, whatever how long the room waited for its players.
     */
    void start();

    /**
     * @brief Runs the simulation steps that are due according to the room's clock.
     *
     * Does nothing while the room is waiting for players. Each step applies the pending inputs, then the systems.
     *
     * @return Number of steps executed (0 if none was due); the `ServerManager` sends a snapshot when it is not 0.
     */
    std::size_t advance();

    /** @brief True once `start()` has been called. */
    [[nodiscard]] bool isPlaying() const { return _playing; }
    /** @brief True when no other player can join. */
    [[nodiscard]] bool isFull() const { return _players.size() >= _maxPlayers; }
    /** @brief True when the last player has left: the `ServerManager` then destroys the room. */
    [[nodiscard]] bool isEmpty() const { return _players.empty(); }
    /**
     * @brief Network ids of the players in this room (lazy view, no allocation).
     *
     * The view is invalidated if a player is added or removed while iterating over it.
     */
    [[nodiscard]] auto players() const { return _players | std::views::keys; }

    /**
     * @brief Sequence number of the last input applied for a player, sent in snapshots for client reconciliation.
     * @param player Network id of the player.
     * @return 0 if no input was applied yet, or if the player is not in the room.
     */
    [[nodiscard]] std::uint32_t lastProcessedInput(ClientId player) const;

    /** @brief Time of the room's last step: `tickCount` numbers the snapshots, from 0 at the start of the game. */
    [[nodiscard]] const ecs::time::Time& time() const { return _clock.time(); }

    /** @brief ECS world of the room. */
    [[nodiscard]] ecs::Registry& registry() { return _registry; }
    /** @brief ECS world of the room, read-only: what the snapshot encoder reads. */
    [[nodiscard]] const ecs::Registry& registry() const { return _registry; }

  private:
    /**
     * @brief One fixed simulation step: applies one pending input per player, then the systems.
     * @param registry ECS world of the room; holds the `ecs::time::Time` resource of this step.
     */
    static void step(ecs::Registry& registry);

    /** @brief Maximum number of players. */
    std::uint8_t _maxPlayers;
    /** @brief Duration of one simulation step, in seconds. */
    float _fixedDt;
    /** @brief Clock of this game, reset by `start()`. */
    ecs::time::Clock _clock;
    /** @brief True once the game has started. */
    bool _playing = false;
    /** @brief ECS world of this game. */
    ecs::Registry _registry;
    /** @brief Event bus of this game, shared by its systems (and the ScriptSystem, once added). */
    ecs::EventBus _events;
    /** @brief Entity controlled by each player of the room; its network state is in its components. */
    std::unordered_map<ClientId, ecs::Entity> _players;
};

}  // namespace rtype::server
