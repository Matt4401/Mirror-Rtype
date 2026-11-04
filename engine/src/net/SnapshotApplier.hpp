#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/NetworkEntityMap.hpp"

namespace ecs::net {

/** @brief An entity created while applying a snapshot. */
struct SpawnedEntity {
    /** @brief New local entity. */
    ecs::Entity entity{};
    /** @brief Its network id. */
    std::uint32_t id = 0;
    /** @brief Its game-defined kind. */
    std::uint16_t kind = 0;
};

/**
 * @brief What a codec uses to write a decoded snapshot into the client world.
 *
 * It resolves network ids to local entities (creating them on first sight), and routes positions to the right place
 * depending on the entity: the target of an `Interpolated` entity, or directly the `Transform` otherwise. It also
 * records which entities the snapshot contained, so that the others can be removed afterwards.
 */
class SnapshotApplier {
  public:
    /**
     * @brief Prepares to apply one snapshot.
     * @param registry Client world.
     * @param map Network id of each replicated entity.
     * @param interval Seconds covered by this snapshot since the previous one, used to interpolate.
     */
    SnapshotApplier(ecs::Registry& registry, NetworkEntityMap& map, float interval);

    /**
     * @brief Local entity of a network id; created with `NetworkId` and `Replicated` if it is unknown.
     * @param id Network id read from the snapshot (untrusted).
     * @param kind Game-defined kind, used when the entity is created.
     * @return The entity, or nothing if `id` is 0 (invalid).
     */
    [[nodiscard]] std::optional<ecs::Entity> entity(std::uint32_t id, std::uint16_t kind);

    /**
     * @brief Writes the server position of an entity.
     *
     * For an `Interpolated` entity (unless it is `Predicted`), sets the new target (the first one is applied at once);
     * otherwise overwrites the `Transform`, adding it if needed. A `Predicted` entity is then reconciled by the plugin.
     *
     * @param entity Entity returned by `entity()`.
     * @param transform State received from the server.
     */
    void setTransform(ecs::Entity entity, const ecs::Transform& transform);

    /** @brief Client world, to write the components the engine does not know about. */
    [[nodiscard]] ecs::Registry& registry() noexcept { return *_registry; }

    /** @brief True if the snapshot contained this entity (returned by `entity()`). */
    [[nodiscard]] bool contains(ecs::Entity entity) const { return _seen.contains(entity); }
    /** @brief Entities created by this snapshot, in creation order. */
    [[nodiscard]] const std::vector<SpawnedEntity>& spawned() const noexcept { return _spawned; }

  private:
    /** @brief Client world. */
    ecs::Registry* _registry;
    /** @brief Network ids of the client world. */
    NetworkEntityMap* _map;
    /** @brief Seconds since the previous snapshot. */
    float _interval;
    /** @brief Entities present in the snapshot. */
    std::unordered_set<ecs::Entity> _seen;
    /** @brief Entities created by the snapshot. */
    std::vector<SpawnedEntity> _spawned;
};

}  // namespace ecs::net
