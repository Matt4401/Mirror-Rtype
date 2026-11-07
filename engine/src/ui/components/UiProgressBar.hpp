#pragma once

#include "components/Renderable.hpp"

namespace ui {

/**
 * @brief Draws a value-based fill bar in screen space.
 *
 * Requires a `UiAnchor` on the same entity to determine screen position.
 * The fill ratio is clamped to [0, 1] and computed as `(value - min) / (max - min)`.
 */
struct UiProgressBar {
    /** @brief Default width of a bar in pixels. */
    static constexpr float DEFAULT_WIDTH = 100.0F;

    /** @brief Default height of a bar in pixels. */
    static constexpr float DEFAULT_HEIGHT = 16.0F;

    /** @brief Default colour of the filled portion (green). */
    static constexpr ecs::Rgba DEFAULT_FOREGROUND = {.r = 0, .g = 200, .b = 0, .a = 255};

    /** @brief Default colour of the empty portion (dark gray). */
    static constexpr ecs::Rgba DEFAULT_BACKGROUND = {.r = 60, .g = 60, .b = 60, .a = 255};

    /** @brief Current value. */
    float value = 0.0F;

    /** @brief Minimum value, maps to an empty bar. */
    float min = 0.0F;

    /** @brief Maximum value, maps to a full bar. */
    float max = 1.0F;

    /** @brief Total width of the bar in pixels. */
    float width = DEFAULT_WIDTH;

    /** @brief Total height of the bar in pixels. */
    float height = DEFAULT_HEIGHT;

    /** @brief Color of the filled portion. */
    ecs::Rgba foreground = DEFAULT_FOREGROUND;

    /** @brief Color of the empty portion. */
    ecs::Rgba background = DEFAULT_BACKGROUND;
};

}  // namespace ui
