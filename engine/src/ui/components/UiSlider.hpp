/**
 * @file UiSlider.hpp
 * @brief Horizontal slider component choosing a value in a range.
 */

#pragma once

#include <string>

#include "components/Renderable.hpp"

namespace ui {

/**
 * @brief Horizontal slider: drag it with the mouse, or move it by `step` with left/right while it has the focus.
 *
 * Every change publishes a `UiValueChangedEvent`.
 */
struct UiSlider {
    /** @brief Default width in pixels. */
    static constexpr float DEFAULT_WIDTH = 200.0F;
    /** @brief Default height in pixels. */
    static constexpr float DEFAULT_HEIGHT = 16.0F;

    /** @brief Current value, kept in `[min, max]`. */
    float value = 0.0F;
    /** @brief Lowest value. */
    float min = 0.0F;
    /** @brief Highest value. */
    float max = 1.0F;
    /** @brief Change applied by one left/right press. */
    float step = 0.1F;
    /** @brief Action name sent with `UiValueChangedEvent`. */
    std::string action;
    /** @brief Width in pixels. */
    float width = DEFAULT_WIDTH;
    /** @brief Height in pixels. */
    float height = DEFAULT_HEIGHT;
    /** @brief Track colour. */
    ecs::Rgba track = {.r = 60, .g = 60, .b = 60, .a = 255};
    /** @brief Filled part colour. */
    ecs::Rgba fill = {.r = 70, .g = 150, .b = 255, .a = 255};
    /** @brief Knob colour. */
    ecs::Rgba knob = {.r = 240, .g = 240, .b = 240, .a = 255};
};

}  // namespace ui
