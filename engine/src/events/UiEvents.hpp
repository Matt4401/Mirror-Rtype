/**
 * @file UiEvents.hpp
 * @brief Events published by the UI widgets.
 */

#pragma once

#include <string>

#include "ecs/Entity.hpp"

namespace ui {

/** @brief A button was clicked, or confirmed while it had the focus. */
struct UiButtonClickedEvent {
    /** @brief Button entity. */
    ecs::Entity entity = 0;
    /** @brief Its `UiButton::action`. */
    std::string action;
};

/** @brief Enter was pressed in a focused text input. */
struct UiTextSubmittedEvent {
    /** @brief Text input entity. */
    ecs::Entity entity = 0;
    /** @brief Its `UiTextInput::action`. */
    std::string action;
    /** @brief Its text. */
    std::string text;
};

/** @brief A slider changed value. */
struct UiValueChangedEvent {
    /** @brief Slider entity. */
    ecs::Entity entity = 0;
    /** @brief Its `UiSlider::action`. */
    std::string action;
    /** @brief New value. */
    float value = 0.0F;
};

/** @brief A toggle was flipped. */
struct UiToggleChangedEvent {
    /** @brief Toggle entity. */
    ecs::Entity entity = 0;
    /** @brief Its `UiToggle::action`. */
    std::string action;
    /** @brief New state. */
    bool on = false;
};

/** @brief The navigation "back" command was pressed (Escape, gamepad B by default). */
struct UiBackEvent {};

}  // namespace ui
