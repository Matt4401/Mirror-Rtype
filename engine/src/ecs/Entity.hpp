#pragma once
#include <cstddef>

namespace ecs {

/**
 * @brief Represents an entity identifier in the Entity Component System.
 *
 * Entities are lightweight numeric identifiers (`std::size_t`).
 * Their state is composed entirely of components stored across SparseArray tables.
 */
using Entity = std::size_t;

}  // namespace ecs
