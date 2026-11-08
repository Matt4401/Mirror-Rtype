/**
 * @file UiScrollList.hpp
 * @brief Vertical list component that only lays out the rows in view.
 */

#pragma once

#include <cstddef>

namespace ui {

/**
 * @brief Vertical list of its `hierarchy` children, of which only `visibleCount` rows are laid out from `first`.
 *
 * Each row in view is anchored inside a slot of `itemHeight` pixels, stacked from the top of the list (the list's own
 * size comes from its `UiRect`, if any). Rows out of view are neither laid out, drawn nor clickable, so a long list
 * costs only its visible rows. The mouse wheel over the list, and focus navigation past its first or last visible row,
 * scroll it.
 */
struct UiScrollList {
    /** @brief Index of the first row in view. */
    std::size_t first = 0;
    /** @brief Number of rows in view. */
    std::size_t visibleCount = 5;
    /** @brief Height of one row slot in pixels. */
    float itemHeight = 40.0F;
};

}  // namespace ui
