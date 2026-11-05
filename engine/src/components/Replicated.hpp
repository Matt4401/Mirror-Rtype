#pragma once

#include <cstdint>

namespace ecs {

/**
 * @brief Marks an entity whose state is sent to the clients in snapshots.
 *
 * `kind` tells the client what to spawn for it. Its values are defined by the game, never by the engine.
 */
struct Replicated {
    /** @brief Game-defined kind of the entity (e.g. a hash of its prefab name, or an index in a table). */
    std::uint32_t kind = 0;
};

}  // namespace ecs
