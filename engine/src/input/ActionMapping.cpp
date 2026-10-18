#include "input/ActionMapping.hpp"

#include <raylib.h>

#include <cstddef>

#include "input/InputAction.hpp"
#include "input/InputConstants.hpp"

namespace input {

ActionMapping::ActionMapping() { resetToDefaults(); }

void ActionMapping::resetToDefaults() {
    _deadzone = DEFAULT_DEADZONE;
    for (auto& binding : _bindings) {
        binding = ActionBinding{};
    }

    // MoveUp (AZERTY Z, QWERTY W, Arrow Up, D-Pad Up, Left Stick Up)
    bindKey(Action::MoveUp, KEY_W);
    bindKey(Action::MoveUp, KEY_Z);
    bindKey(Action::MoveUp, KEY_UP);
    bindGamepadButton(Action::MoveUp, GAMEPAD_BUTTON_LEFT_FACE_UP);
    bindGamepadAxis(Action::MoveUp, GAMEPAD_AXIS_LEFT_Y, -1.0F);

    // MoveDown (S, Arrow Down, D-Pad Down, Left Stick Down)
    bindKey(Action::MoveDown, KEY_S);
    bindKey(Action::MoveDown, KEY_DOWN);
    bindGamepadButton(Action::MoveDown, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    bindGamepadAxis(Action::MoveDown, GAMEPAD_AXIS_LEFT_Y, 1.0F);

    // MoveLeft (AZERTY Q, QWERTY A, Arrow Left, D-Pad Left, Left Stick Left)
    bindKey(Action::MoveLeft, KEY_A);
    bindKey(Action::MoveLeft, KEY_Q);
    bindKey(Action::MoveLeft, KEY_LEFT);
    bindGamepadButton(Action::MoveLeft, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bindGamepadAxis(Action::MoveLeft, GAMEPAD_AXIS_LEFT_X, -1.0F);

    // MoveRight (D, Arrow Right, D-Pad Right, Left Stick Right)
    bindKey(Action::MoveRight, KEY_D);
    bindKey(Action::MoveRight, KEY_RIGHT);
    bindGamepadButton(Action::MoveRight, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bindGamepadAxis(Action::MoveRight, GAMEPAD_AXIS_LEFT_X, 1.0F);

    // Shoot (Space, Enter, Gamepad A / Cross)
    bindKey(Action::Shoot, KEY_SPACE);
    bindKey(Action::Shoot, KEY_ENTER);
    bindGamepadButton(Action::Shoot, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

    // ChargeWeapon (Shift, Gamepad Right Trigger)
    bindKey(Action::ChargeWeapon, KEY_LEFT_SHIFT);
    bindKey(Action::ChargeWeapon, KEY_RIGHT_SHIFT);
    bindGamepadButton(Action::ChargeWeapon, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
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
