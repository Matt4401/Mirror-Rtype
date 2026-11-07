#pragma once

#include "components/Renderable.hpp"

namespace ui {

/**
 * @brief Draws a filled rectangle with an optional border in screen space.
 *
 * Requires a `UiAnchor` on the same entity to determine screen position.
 * Uses `ecs::Rgba` for colors, keeping this component free of any graphics-library types.
 */
struct UiRect {
    /** @brief Width of the rectangle in pixels. */
    float width = 0.0F;

    /** @brief Height of the rectangle in pixels. */
    float height = 0.0F;

    /** @brief Fill color; fully transparent disables the fill. */
    ecs::Rgba fillColor;

    /** @brief Border color; fully transparent disables the border. */
    ecs::Rgba borderColor = {.r = 0, .g = 0, .b = 0, .a = 0};

    /** @brief Border thickness in pixels; 0 disables the border. */
    float borderThickness = 0.0F;
};

}  // namespace ui
