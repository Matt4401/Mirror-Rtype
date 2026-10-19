#pragma once

#include "input/InputAction.hpp"

namespace events {

/**
 * @brief Event published when an action transitions state.
 *
 * Designed to be serialized over UDP to the server or consumed locally.
 */
struct InputActionEvent {
    /** @brief The logical input action triggered. */
    input::Action action = input::Action::MoveUp;

    /** @brief Transition state of the action. */
    input::ActionState state = input::ActionState::Pressed;

    /** @brief Analog or digital magnitude of the action. */
    float value = 1.0F;

    /** @brief Index of the player originating the action. */
    int playerIndex = 0;
};

}  // namespace events
