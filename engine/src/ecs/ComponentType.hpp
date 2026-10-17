#pragma once
#include <cstddef>

namespace ecs {

/**
 * @brief Numeric sequential identifier assigned to each component type.
 */
using ComponentTypeId = std::size_t;

namespace detail {

/**
 * @brief Generates sequential integer IDs starting from 0.
 * @return Monotonically increasing ComponentTypeId.
 */
[[nodiscard]] inline ComponentTypeId getNextComponentTypeId() noexcept {
    static ComponentTypeId counter = 0;
    return counter++;
}

}  // namespace detail

/**
 * @brief Retrieves a unique, sequential numeric ID for a component type.
 *
 * Initialized once upon first call and cached in a function-local static variable.
 * Subsequent calls resolve in a single memory load with zero hash overhead.
 *
 * @tparam Component The component aggregate struct.
 * @return Unique numeric identifier (0, 1, 2, ...).
 */
template <typename Component>
[[nodiscard]] inline ComponentTypeId getComponentTypeId() noexcept {
    static const ComponentTypeId id = detail::getNextComponentTypeId();
    return id;
}

}  // namespace ecs
