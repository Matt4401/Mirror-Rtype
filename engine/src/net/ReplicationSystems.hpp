#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "components/InputBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/IReplicationCodec.hpp"

// Systems of the replication plugins, as free functions so that they can run (and be tested) without a plugin.
namespace ecs::net {

/**
 * @brief Gives an entity a network id and marks it as replicated (server side).
 *
 * Creates the `NetworkEntityMap` resource if needed.
 *
 * @param registry Server world.
 * @param entity Entity to replicate.
 * @param kind Game-defined kind, sent to the clients so that they know what to spawn.
 * @return Its network id.
 */
std::uint32_t replicate(ecs::Registry& registry, ecs::Entity entity, std::uint32_t kind);

/**
 * @brief Server: finds the entity controlled by a peer, i.e. carrying `NetworkOwner{peer}` and an `InputBuffer`.
 * @param registry Server world.
 * @param peer Owner to look for.
 * @return The first such entity, or nothing.
 */
[[nodiscard]] std::optional<ecs::Entity> findOwnedEntity(ecs::Registry& registry, PeerId peer);

/**
 * @brief Server: queues an input in the `InputBuffer` of the entity controlled by the peer.
 * @param registry Server world.
 * @param peer Sender of the input.
 * @param input Decoded input.
 * @return False if the peer controls no entity or the input was dropped (late, duplicated).
 */
bool receiveInput(ecs::Registry& registry, PeerId peer, const NetInput& input);

/**
 * @brief Server, once per fixed step: takes one pending input per entity and publishes it as `InputApplied`.
 * @param registry Server world.
 * @param events Bus the game's input handlers listen to.
 * @param deltaTime Duration of the step, in seconds.
 */
void applyInputs(ecs::Registry& registry, ecs::EventBus& events, float deltaTime);

/**
 * @brief Server: encodes the replicated entities and sends one snapshot to each peer controlling an entity.
 *
 * Each peer receives the sequence of the last of its inputs applied by the server. Payloads that are empty or larger
 * than `MAX_UNRELIABLE_PAYLOAD` are not sent.
 *
 * @param registry Server world.
 * @param codec Game wire format.
 * @param service Where snapshots are sent (unreliable channel).
 * @param tick Server step of the snapshot.
 * @return Number of snapshots sent.
 */
std::size_t sendSnapshots(ecs::Registry& registry, IReplicationCodec& codec, engine::INetworkService& service,
                          std::uint32_t tick);

/**
 * @brief Client: removes the replicated entities a snapshot did not contain. Publishes `ReplicatedEntityDespawned`
 * for each of them before killing it.
 * @param registry Client world.
 * @param events Bus the game listens to.
 * @param applier Applier of the snapshot just applied.
 */
void despawnMissing(ecs::Registry& registry, ecs::EventBus& events, const SnapshotApplier& applier);

/**
 * @brief Client: reconciles the `Predicted` entities with the server state just applied.
 *
 * Drops the inputs the server has already processed, then publishes `InputApplied{replayed = true}` for each of the
 * others, oldest first: the game re-applies them on top of the server state.
 *
 * @param registry Client world.
 * @param events Bus the game's input handlers listen to.
 * @param applier Applier of the snapshot just applied; only the entities it contained are reconciled.
 * @param lastProcessedInput Sequence acknowledged by the server.
 * @param deltaTime Duration of one input, in seconds.
 */
void reconcile(ecs::Registry& registry, ecs::EventBus& events, const SnapshotApplier& applier,
               std::uint32_t lastProcessedInput, float deltaTime);

/**
 * @brief Client: records the local input in every `Predicted` entity and publishes `InputApplied` for each of them.
 * @param registry Client world.
 * @param events Bus the game's input handlers listen to.
 * @param input Input sent this step.
 * @param deltaTime Duration of one input, in seconds.
 */
void predict(ecs::Registry& registry, ecs::EventBus& events, const NetInput& input, float deltaTime);

/**
 * @brief Client, once per frame: moves each `Interpolated` entity's `Transform` toward its last snapshot.
 *
 * `Predicted` entities are skipped: their `Transform` is driven by the local inputs.
 * @param registry Client world.
 * @param deltaTime Frame duration, in seconds.
 */
void interpolate(ecs::Registry& registry, float deltaTime);

}  // namespace ecs::net
