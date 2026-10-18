#pragma once

#include "input/InputAction.hpp"

namespace events {

/**
 * @brief Event published when an action transitions state.
 *
 * Designed to be serialized over UDP to the server or consumed locally.
 */
struct InputActionEvent {
    input::Action action = input::Action::MoveUp;
    input::ActionState state = input::ActionState::Pressed;
    float value = 1.0F;
    int playerIndex = 0;
};

}  // namespace events
