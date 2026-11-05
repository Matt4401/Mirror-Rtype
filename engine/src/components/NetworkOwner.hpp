#pragma once

#include <cstdint>

namespace ecs {

/**
 * @brief Remote peer that controls an entity (its inputs drive it).
 *
 * Lets systems find the entity of a client, or the client of an entity, without a side table outside the ECS.
 */
struct NetworkOwner {
    /** @brief Id of the owning peer, as given by the network layer. */
    std::uint32_t client = 0;
};

}  // namespace ecs
