/**
 * @file UiButton.hpp
 * @brief Clickable HUD button component with per-state colours and an action name.
 */

#pragma once

#include <cstdint>
#include <string>

#include "components/Renderable.hpp"

namespace ui {

/** @brief Interaction state of a UiButton. */
enum class UiButtonState : std::uint8_t {
    Idle,
    Hovered,
    Pressed,
    Released,
};

/**
 * @brief Clickable HUD button rendered in screen space.
 *
 * Requires a `UiAnchor` on the same entity to determine screen position.
 * `UiButtonSystem` updates `state` each frame and dispatches a
 * `UiButtonClickedEvent` on the frame the button transitions to `Released`.
 */
struct UiButton {
    /** @brief Width of the button in pixels. */
    float width = 120.0F;

    /** @brief Height of the button in pixels. */
    float height = 36.0F;

    /** @brief Label text drawn centred inside the button. */
    std::string label;

    /**
     * @brief Logical action name dispatched with the click event.
     *
     * Listeners can filter `UiButtonClickedEvent` by `action` to react
     * to specific buttons without coupling to entity identifiers.
     */
    std::string action;

    /** @brief Background colour when the cursor is not over the button. */
    ecs::Rgba colorIdle = {.r = 80, .g = 80, .b = 80, .a = 255};

    /** @brief Background colour when the cursor hovers over the button. */
    ecs::Rgba colorHovered = {.r = 120, .g = 120, .b = 120, .a = 255};

    /** @brief Background colour while the left mouse button is held. */
    ecs::Rgba colorPressed = {.r = 50, .g = 50, .b = 50, .a = 255};

    /** @brief Label text colour. */
    ecs::Rgba colorText = {.r = 255, .g = 255, .b = 255, .a = 255};

    /** @brief Font size for the label in pixels. */
    float fontSize = 20.0F;

    /** @brief Current interaction state; updated by UiButtonSystem. */
    UiButtonState state = UiButtonState::Idle;
};

}  // namespace ui
