#pragma once
#include <cstddef>

namespace ecs {

/**
 * @brief Numeric sequential identifier assigned to each resource type.
 */
using ResourceTypeId = std::size_t;

namespace detail {

/**
 * @brief Generates sequential integer IDs starting from 0 for resource types.
 * @return Monotonically increasing ResourceTypeId.
 */
[[nodiscard]] inline ResourceTypeId getNextResourceTypeId() noexcept {
    static ResourceTypeId counter = 0;
    return counter++;
}

}  // namespace detail

/**
 * @brief Retrieves a unique, sequential numeric ID for a resource type.
 *
 * Initialized once upon first call and cached in a function-local static variable.
 * Subsequent calls resolve in a single memory load with zero hash overhead.
 *
 * @tparam Resource The resource type.
 * @return Unique numeric identifier (0, 1, 2, ...).
 */
template <typename Resource>
[[nodiscard]] inline ResourceTypeId getResourceTypeId() noexcept {
    static const ResourceTypeId id = detail::getNextResourceTypeId();
    return id;
}

}  // namespace ecs
