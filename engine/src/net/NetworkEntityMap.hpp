#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "ecs/Entity.hpp"

namespace ecs::net {

/**
 * @brief Registry resource mapping network ids to local entities, both ways.
 *
 * The server calls `assign()` to give each replicated entity a new id; a client calls `bind()` with the id read from
 * a snapshot. Call `unbindEntity()` when a mapped entity is killed, so that its id is not resolved to a recycled
 * entity.
 */
class NetworkEntityMap {
  public:
    /**
     * @brief Gives an entity a new network id (server side). Ids start at 1 and are never reused.
     * @param entity Local entity.
     * @return The new id, or the entity's current id if it already has one.
     */
    std::uint32_t assign(ecs::Entity entity);

    /**
     * @brief Maps an id received from the server to a local entity (client side).
     *
     * Replaces any previous mapping of this id or of this entity.
     *
     * @param id Network id; 0 is ignored.
     * @param entity Local entity.
     * @return False if `id` is 0.
     */
    bool bind(std::uint32_t id, ecs::Entity entity);

    /** @brief Removes the mapping of a network id. Does nothing if it is unknown. */
    void unbind(std::uint32_t id);
    /** @brief Removes the mapping of a local entity. Does nothing if it is unknown. */
    void unbindEntity(ecs::Entity entity);

    /** @brief Local entity of a network id, or nothing if unknown. */
    [[nodiscard]] std::optional<ecs::Entity> entityOf(std::uint32_t id) const;
    /** @brief Network id of a local entity, or nothing if unknown. */
    [[nodiscard]] std::optional<std::uint32_t> idOf(ecs::Entity entity) const;
    /** @brief Number of mapped entities. */
    [[nodiscard]] std::size_t size() const noexcept { return _entities.size(); }

  private:
    /** @brief Local entity of each network id. */
    std::unordered_map<std::uint32_t, ecs::Entity> _entities;
    /** @brief Network id of each local entity. */
    std::unordered_map<ecs::Entity, std::uint32_t> _ids;
    /** @brief Id given by the next `assign()`. */
    std::uint32_t _nextId = 1;
};

}  // namespace ecs::net
