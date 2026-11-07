#pragma once

#include <cstdint>

namespace ecs {

/**
 * @brief Identifier of an entity shared by the server and every client.
 *
 * The local `ecs::Entity` of a replicated object differs on each machine (each registry allocates and recycles its
 * own ids), so the network refers to entities by this id instead. The server assigns it (see
 * `ecs::net::NetworkEntityMap::assign`); clients map it back to their local entity.
 */
struct NetworkId {
    /** @brief Network-wide id; 0 is never assigned and means "no id". */
    std::uint32_t value = 0;
};

}  // namespace ecs
