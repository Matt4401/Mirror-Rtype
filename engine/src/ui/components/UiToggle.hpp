/**
 * @file UiToggle.hpp
 * @brief On/off switch component.
 */

#pragma once

#include <string>

#include "components/Renderable.hpp"

namespace ui {

/**
 * @brief Check box: a click, or confirm while it has the focus, flips `on` and publishes a `UiToggleChangedEvent`.
 *
 * Put a `UiText` child next to it for its label.
 */
struct UiToggle {
    /** @brief Default size of the box in pixels. */
    static constexpr float DEFAULT_SIZE = 28.0F;

    /** @brief Current state. */
    bool on = false;
    /** @brief Action name sent with `UiToggleChangedEvent`. */
    std::string action;
    /** @brief Width of the box in pixels. */
    float width = DEFAULT_SIZE;
    /** @brief Height of the box in pixels. */
    float height = DEFAULT_SIZE;
    /** @brief Box colour. */
    ecs::Rgba box = {.r = 60, .g = 60, .b = 60, .a = 255};
    /** @brief Mark colour, drawn while `on`. */
    ecs::Rgba mark = {.r = 70, .g = 150, .b = 255, .a = 255};
};

}  // namespace ui
