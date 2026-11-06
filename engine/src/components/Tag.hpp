/**
 * @file Tag.hpp
 * @brief Generic semantic label attached to an entity (e.g. "player", "enemy").
 */

#pragma once

#include <string>

namespace ecs {

/**
 * @brief Free-form label used to find entities by role, for example from scripts with `World.getNearestWithTag`.
 *
 * The engine gives no meaning to the string: each game picks its own vocabulary. Matching is exact and
 * case-sensitive. An entity carries at most one tag.
 */
struct Tag {
    /** @brief Label of the entity; compared exactly by the spatial queries. */
    std::string name;
};

}  // namespace ecs
