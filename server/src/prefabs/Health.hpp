/**
 * @file Health.hpp
 * @brief Hit points of an R-Type entity that survives several hits.
 */

#pragma once

#include <cstdint>

namespace prefabs {

/**
 * @brief Hit points of an entity that takes several hits to die, such as a boss.
 *
 * An entity without it dies on the first hit. The gameplay removes one point per player shot and destroys the entity
 * when `points` reaches 0. Kept within `std::uint8_t` so it fits `EntityNetState::health` as is.
 */
struct Health {
    /** @brief Hit points left. */
    std::uint8_t points = 1;
    /** @brief Hit points at spawn, for health bars. */
    std::uint8_t max = 1;
};

}  // namespace prefabs
