#include "input/ActionMapping.hpp"

#include <raylib.h>

#include <cstddef>

#include "input/InputAction.hpp"
#include "input/InputConstants.hpp"

namespace input {

ActionMapping::ActionMapping() = default;

void ActionMapping::clear() {
    _deadzone = DEFAULT_DEADZONE;
    for (auto& binding : _bindings) {
        binding = ActionBinding{};
    }
}

const ActionBinding& ActionMapping::getBinding(Action action) const {
    return _bindings.at(static_cast<std::size_t>(action));
}

bool ActionMapping::hasKey(Action action, int key) const {
    const ActionBinding& binding = getBinding(action);
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (binding.keys.at(i) == key) {
            return true;
        }
    }
    return false;
}

bool ActionMapping::bindKey(Action action, int key) {
    if (key <= 0 || hasKey(action, key)) {
        return false;
    }
    ActionBinding& binding = _bindings.at(static_cast<std::size_t>(action));
    if (binding.keyCount >= MAX_KEY_BINDINGS) {
        return false;
    }
    binding.keys.at(binding.keyCount++) = key;
    return true;
}

bool ActionMapping::unbindKey(Action action, int key) {
    ActionBinding& binding = _bindings.at(static_cast<std::size_t>(action));
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (binding.keys.at(i) == key) {
            for (std::size_t j = i; j + 1 < binding.keyCount; ++j) {
                binding.keys.at(j) = binding.keys.at(j + 1);
            }
            binding.keys.at(--binding.keyCount) = 0;
            return true;
        }
    }
    return false;
}

void ActionMapping::clearKeyBindings(Action action) {
    ActionBinding& binding = _bindings.at(static_cast<std::size_t>(action));
    binding.keyCount = 0;
    binding.keys.fill(0);
}

bool ActionMapping::bindGamepadButton(Action action, int button) {
    if (button < 0) {
        return false;
    }
    ActionBinding& binding = _bindings.at(static_cast<std::size_t>(action));
    for (std::size_t i = 0; i < binding.padButtonCount; ++i) {
        if (binding.padButtons.at(i) == button) {
            return false;
        }
    }
    if (binding.padButtonCount >= MAX_PAD_BUTTON_BINDINGS) {
        return false;
    }
    binding.padButtons.at(binding.padButtonCount++) = button;
    return true;
}

void ActionMapping::bindGamepadAxis(Action action, int axis, float direction) {
    ActionBinding& binding = _bindings.at(static_cast<std::size_t>(action));
    binding.padAxis = axis;
    binding.padAxisDirection = direction;
}

bool ActionMapping::isActionDown(Action action, int gamepadIndex) const {
    if (!IsWindowReady()) {
        return false;
    }
    const ActionBinding& binding = getBinding(action);
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (IsKeyDown(binding.keys.at(i))) {
            return true;
        }
    }
    if (IsGamepadAvailable(gamepadIndex)) {
        for (std::size_t i = 0; i < binding.padButtonCount; ++i) {
            if (IsGamepadButtonDown(gamepadIndex, binding.padButtons.at(i))) {
                return true;
            }
        }
        if (binding.padAxis >= 0) {
            const float val = GetGamepadAxisMovement(gamepadIndex, binding.padAxis);
            if (binding.padAxisDirection > 0.0F && val > _deadzone) {
                return true;
            }
            if (binding.padAxisDirection < 0.0F && val < -_deadzone) {
                return true;
            }
        }
    }
    return false;
}

bool ActionMapping::isActionPressed(Action action, int gamepadIndex) const {
    if (!IsWindowReady()) {
        return false;
    }
    const ActionBinding& binding = getBinding(action);
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (IsKeyPressed(binding.keys.at(i))) {
            return true;
        }
    }
    if (IsGamepadAvailable(gamepadIndex)) {
        for (std::size_t i = 0; i < binding.padButtonCount; ++i) {
            if (IsGamepadButtonPressed(gamepadIndex, binding.padButtons.at(i))) {
                return true;
            }
        }
    }
    return false;
}

bool ActionMapping::isActionReleased(Action action, int gamepadIndex) const {
    if (!IsWindowReady()) {
        return false;
    }
    const ActionBinding& binding = getBinding(action);
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (IsKeyReleased(binding.keys.at(i))) {
            return true;
        }
    }
    if (IsGamepadAvailable(gamepadIndex)) {
        for (std::size_t i = 0; i < binding.padButtonCount; ++i) {
            if (IsGamepadButtonReleased(gamepadIndex, binding.padButtons.at(i))) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace input
