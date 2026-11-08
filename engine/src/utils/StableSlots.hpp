/**
 * @file StableSlots.hpp
 * @brief Slot vectors whose handles stay stable: an asset loaded again can get back the handle it had.
 *
 * Used by the stores of the render and audio modules. Handle `n` is slot `n - 1`; `0` is "none". A fresh load always
 * takes a new slot at the end, so a handle is never given to another asset by accident; only a load asking for its
 * former handle (`preferred`) reuses a released slot.
 */

#pragma once

#include <concepts>
#include <cstddef>
#include <utility>
#include <vector>

namespace ecs {

/** @brief Most empty slots a `preferred` handle or a reservation may add past the end of a store. */
inline constexpr std::size_t MAX_SLOT_GAP = 4096;

/**
 * @brief Stores a loaded slot, in the slot of `preferred` when that one is free.
 *
 * @tparam Slot Slot type with a `bool loaded` member; a default-constructed one is free.
 * @tparam Id Unsigned handle type.
 * @param slots Slots.
 * @param slot Slot to store.
 * @param preferred Handle to give back, or 0.
 * @return Handle of the stored slot.
 */
template <typename Slot, std::unsigned_integral Id>
[[nodiscard]] Id placeInSlot(std::vector<Slot>& slots, Slot slot, const Id preferred) {
    const auto wanted = static_cast<std::size_t>(preferred);
    if (wanted != 0 && wanted <= slots.size() + MAX_SLOT_GAP) {
        if (wanted > slots.size()) {
            slots.resize(wanted);
        }
        if (!slots[wanted - 1].loaded) {
            slots[wanted - 1] = std::move(slot);
            return preferred;
        }
    }
    slots.push_back(std::move(slot));
    return static_cast<Id>(slots.size());
}

/**
 * @brief Keeps every handle up to `last` out of the fresh allocations, by adding free slots up to it.
 *
 * @tparam Slot Slot type, default-constructed free.
 * @tparam Id Unsigned handle type.
 * @param slots Slots.
 * @param last Highest handle to keep.
 */
template <typename Slot, std::unsigned_integral Id>
void reserveSlots(std::vector<Slot>& slots, const Id last) {
    const auto wanted = static_cast<std::size_t>(last);
    if (wanted > slots.size() && wanted <= slots.size() + MAX_SLOT_GAP) {
        slots.resize(wanted);
    }
}

}  // namespace ecs
