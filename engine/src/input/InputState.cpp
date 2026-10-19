#include "input/InputState.hpp"

#include <cstddef>

#include "input/InputAction.hpp"

namespace input {

bool InputState::isDown(Action action) const noexcept { return _held.test(static_cast<std::size_t>(action)); }

bool InputState::isJustPressed(Action action) const noexcept {
    return _justPressed.test(static_cast<std::size_t>(action));
}

bool InputState::isJustReleased(Action action) const noexcept {
    return _justReleased.test(static_cast<std::size_t>(action));
}

float InputState::axisX() const noexcept { return _axisX; }

float InputState::axisY() const noexcept { return _axisY; }

void InputState::setDown(Action action, bool down) noexcept { _held.set(static_cast<std::size_t>(action), down); }

void InputState::setJustPressed(Action action, bool pressed) noexcept {
    _justPressed.set(static_cast<std::size_t>(action), pressed);
}

void InputState::setJustReleased(Action action, bool released) noexcept {
    _justReleased.set(static_cast<std::size_t>(action), released);
}

void InputState::setAxes(float x, float y) noexcept {
    _axisX = x;
    _axisY = y;
}

void InputState::resetFrameTransitions() noexcept {
    _justPressed.reset();
    _justReleased.reset();
}

}  // namespace input
