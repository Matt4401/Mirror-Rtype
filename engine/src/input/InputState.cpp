#include "input/InputState.hpp"

#include <cmath>

namespace input {

bool InputState::isDown(ActionId action) const noexcept {
    const auto it = actions.find(action);
    return (it != actions.end()) ? it->second.held : false;
}

bool InputState::isJustPressed(ActionId action) const noexcept {
    const auto it = actions.find(action);
    return (it != actions.end()) ? it->second.justPressed : false;
}

bool InputState::isJustReleased(ActionId action) const noexcept {
    const auto it = actions.find(action);
    return (it != actions.end()) ? it->second.justReleased : false;
}

float InputState::getAxis(ActionId action) const noexcept {
    const auto it = actions.find(action);
    return (it != actions.end()) ? it->second.axis : 0.0F;
}

AxisVector2 InputState::getVector2(ActionId axisX, ActionId axisY, bool normalize) const noexcept {
    float x = getAxis(axisX);
    float y = getAxis(axisY);
    if (normalize) {
        const float lenSq = (x * x) + (y * y);
        if (lenSq > 1.0F) {
            const float invLen = 1.0F / std::sqrt(lenSq);
            x *= invLen;
            y *= invLen;
        }
    }
    return AxisVector2{.x = x, .y = y};
}

void InputState::setDown(ActionId action, bool down) { actions[action].held = down; }

void InputState::setJustPressed(ActionId action, bool pressed) { actions[action].justPressed = pressed; }

void InputState::setJustReleased(ActionId action, bool released) { actions[action].justReleased = released; }

void InputState::setAxis(ActionId action, float value) { actions[action].axis = value; }

void InputState::resetFrameTransitions() noexcept {
    for (auto& [id, actionData] : actions) {
        actionData.justPressed = false;
        actionData.justReleased = false;
    }
}

void InputState::clear() noexcept { actions.clear(); }

}  // namespace input
