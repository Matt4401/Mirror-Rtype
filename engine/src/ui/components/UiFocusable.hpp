/**
 * @file UiFocusable.hpp
 * @brief Component letting keyboard and gamepad navigation reach a widget.
 */

#pragma once

namespace ui {

/**
 * @brief Marks a widget (button, toggle, slider, text input) as reachable by focus navigation.
 *
 * Navigation goes through the visible focusable widgets by increasing `order`, then by layout order (parents before
 * children, siblings in insertion order). Clicking a focusable widget also focuses it.
 */
struct UiFocusable {
    /** @brief Navigation rank; lower comes first, equal ranks keep the layout order. */
    int order = 0;
};

}  // namespace ui
