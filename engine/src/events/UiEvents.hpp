/**
 * @file UiEvents.hpp
 * @brief Events dispatched by the UI subsystem.
 */

#pragma once

#include <string>

#include "ecs/Entity.hpp"

namespace ui {

/**
 * @brief Fired on the frame a `UiButton` transitions from Pressed to Released.
 *
 * Subscribe via `EventBus::subscribe<UiButtonClickedEvent>(...)` to react to
 * button clicks without coupling to specific entity identifiers.
 */
struct UiButtonClickedEvent {
    /** @brief Entity owning the clicked button component. */
    ecs::Entity entity = 0;

    /** @brief Action name copied from `UiButton::action`. */
    std::string action;
};

}  // namespace ui
