#pragma once

#include <cstdint>

#include "components/InputBuffer.hpp"
#include "ecs/Entity.hpp"

// Events published on the engine EventBus by the replication plugins.
namespace ecs::net {

/**
 * @brief An input must be applied to the entity it drives.
 *
 * Published on the server for each input taken from an `InputBuffer` (one per entity and per fixed step), and on the
 * client for each input of a `Predicted` entity, first when it is sent, then again on reconciliation. The game moves
 * the entity in its handler: using the same handler on both sides keeps the prediction equal to the server result.
 */
struct InputApplied {
    /** @brief Entity driven by the input. */
    ecs::Entity entity{};
    /** @brief Input to apply; `actions` is game-defined. */
    NetInput input{};
    /** @brief Duration of the step covered by this input, in seconds. */
    float deltaTime = 0.0F;
    /** @brief True when the client replays an input after a snapshot (no sound or effect should be played again). */
    bool replayed = false;
};

/**
 * @brief Client: the local player's input of this step. Publish it from the game, once per fixed step.
 *
 * `ClientReplicationPlugin` sends it to the server and applies it right away to every `Predicted` entity.
 */
struct LocalInput {
    /** @brief Input of this step; its sequence must increase with every input. */
    NetInput input{};
};

/** @brief Client: a snapshot created a local entity. Published once the whole snapshot has been applied. */
struct ReplicatedEntitySpawned {
    /** @brief New local entity, already carrying `NetworkId`, `Replicated` and the state written by the codec. */
    ecs::Entity entity{};
    /** @brief Network id of the entity. */
    std::uint32_t id = 0;
    /** @brief Game-defined kind, to choose its sprite, sound... */
    std::uint32_t kind = 0;
};

/** @brief Client: a replicated entity is no longer in the snapshots. Published just before it is killed. */
struct ReplicatedEntityDespawned {
    /** @brief Local entity, still alive while the event is handled. */
    ecs::Entity entity{};
    /** @brief Network id of the entity. */
    std::uint32_t id = 0;
    /** @brief Game-defined kind of the entity. */
    std::uint32_t kind = 0;
};

}  // namespace ecs::net
