#pragma once

#include "ecs/Entity.hpp"

namespace systems {

/** @brief Two colliders started touching. Published after the physics step, so handlers may kill entities. */
struct CollisionEvent {
    /** @brief First entity of the contact. */
    ecs::Entity a{};
    /** @brief Second entity of the contact. */
    ecs::Entity b{};
};
}  // namespace systems
