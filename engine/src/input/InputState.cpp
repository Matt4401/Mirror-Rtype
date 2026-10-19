#include "input/InputState.hpp"

namespace input {

bool InputState::isDown(ActionId action) const noexcept {
    const auto it = _held.find(action);
    return (it != _held.end()) ? it->second : false;
}

bool InputState::isJustPressed(ActionId action) const noexcept {
    const auto it = _justPressed.find(action);
    return (it != _justPressed.end()) ? it->second : false;
}

bool InputState::isJustReleased(ActionId action) const noexcept {
    const auto it = _justReleased.find(action);
    return (it != _justReleased.end()) ? it->second : false;
}

float InputState::getAxis(ActionId action) const noexcept {
    const auto it = _axes.find(action);
    return (it != _axes.end()) ? it->second : 0.0F;
}

void InputState::setDown(ActionId action, bool down) { _held[action] = down; }

void InputState::setJustPressed(ActionId action, bool pressed) { _justPressed[action] = pressed; }

void InputState::setJustReleased(ActionId action, bool released) { _justReleased[action] = released; }

void InputState::setAxis(ActionId action, float value) { _axes[action] = value; }

void InputState::resetFrameTransitions() noexcept {
    _justPressed.clear();
    _justReleased.clear();
}

void InputState::clear() noexcept {
    _held.clear();
    _justPressed.clear();
    _justReleased.clear();
    _axes.clear();
}

}  // namespace input
