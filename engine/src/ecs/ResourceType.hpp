#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>

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
    static std::atomic<ResourceTypeId> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

template <typename CleanResource>
[[nodiscard]] inline ResourceTypeId getCleanResourceTypeId() noexcept {
    static const ResourceTypeId id = getNextResourceTypeId();
    return id;
}

}  // namespace detail

/**
 * @brief Retrieves a unique, sequential numeric ID for a resource type.
 *
 * Initialized once upon first call and cached in a function-local static variable.
 * Subsequent calls resolve in a single memory load with zero hash overhead.
 * Automatically strips references and cv-qualifiers so that T, const T, T&, etc.
 * all resolve to the exact same identifier.
 *
 * @tparam Resource The resource type.
 * @return Unique numeric identifier (0, 1, 2, ...).
 */
template <typename Resource>
[[nodiscard]] inline ResourceTypeId getResourceTypeId() noexcept {
    return detail::getCleanResourceTypeId<std::remove_cvref_t<Resource>>();
}

}  // namespace ecs
