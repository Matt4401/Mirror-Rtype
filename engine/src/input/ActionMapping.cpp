#include "input/ActionMapping.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "input/ActionId.hpp"
#include "input/InputConstants.hpp"

namespace input {

float ActionMapping::sampleKeyboardComposite(const ActionBinding& binding) {
    float value = 0.0F;
    for (std::size_t i = 0; i < binding.negativeKeyCount; ++i) {
        if (IsKeyDown(binding.negativeKeys.at(i))) {
            value -= 1.0F;
            break;
        }
    }
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (IsKeyDown(binding.keys.at(i))) {
            value += 1.0F;
            break;
        }
    }
    return value;
}

float ActionMapping::sampleGamepadButtonComposite(const ActionBinding& binding, int gamepadIndex) {
    float value = 0.0F;
    for (std::size_t i = 0; i < binding.negativePadButtonCount; ++i) {
        if (IsGamepadButtonDown(gamepadIndex, binding.negativePadButtons.at(i))) {
            value -= 1.0F;
            break;
        }
    }
    for (std::size_t i = 0; i < binding.padButtonCount; ++i) {
        if (IsGamepadButtonDown(gamepadIndex, binding.padButtons.at(i))) {
            value += 1.0F;
            break;
        }
    }
    return value;
}

float ActionMapping::sampleGamepadStick(const ActionBinding& binding, int gamepadIndex, float deadzone) {
    if (binding.padAxis < 0) {
        return 0.0F;
    }
    const float raw = GetGamepadAxisMovement(gamepadIndex, binding.padAxis);
    if (std::abs(raw) > deadzone) {
        return raw * binding.padAxisDirection;
    }
    return 0.0F;
}

ActionMapping::ActionMapping() = default;

ActionId ActionMapping::registerAction(std::string_view name, ActionType type) {
    const ActionId id = makeActionId(name);
    auto it = _bindings.find(id);
    if (it == _bindings.end()) {
        _bindings[id] = ActionBinding{
            .type = type,
            .name = std::string(name),
            .keys = {0, 0, 0, 0},
            .keyCount = 0,
            .negativeKeys = {0, 0, 0, 0},
            .negativeKeyCount = 0,
            .padButtons = {-1, -1},
            .padButtonCount = 0,
            .negativePadButtons = {-1, -1},
            .negativePadButtonCount = 0,
            .padAxis = -1,
            .padAxisDirection = 1.0F,
        };
    } else {
        it->second.type = type;
        it->second.name = std::string(name);
    }
    return id;
}

ActionBinding& ActionMapping::getOrCreateBinding(ActionId action) { return _bindings[action]; }

const ActionBinding* ActionMapping::getBinding(ActionId action) const {
    const auto it = _bindings.find(action);
    if (it != _bindings.end()) {
        return &it->second;
    }
    return nullptr;
}

void ActionMapping::clear() {
    _deadzone = DEFAULT_DEADZONE;
    _bindings.clear();
}

bool ActionMapping::hasKey(ActionId action, int key) const {
    const ActionBinding* binding = getBinding(action);
    if (binding == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < binding->keyCount; ++i) {
        if (binding->keys.at(i) == key) {
            return true;
        }
    }
    for (std::size_t i = 0; i < binding->negativeKeyCount; ++i) {
        if (binding->negativeKeys.at(i) == key) {
            return true;
        }
    }
    return false;
}

bool ActionMapping::bindKey(ActionId action, int key) {
    if (key <= 0 || hasKey(action, key)) {
        return false;
    }
    ActionBinding& binding = getOrCreateBinding(action);
    if (binding.keyCount >= MAX_KEY_BINDINGS) {
        return false;
    }
    binding.keys.at(binding.keyCount++) = key;
    return true;
}

bool ActionMapping::unbindKey(ActionId action, int key) {
    const auto it = _bindings.find(action);
    if (it == _bindings.end()) {
        return false;
    }
    ActionBinding& binding = it->second;
    bool found = false;
    for (std::size_t i = 0; i < binding.keyCount; ++i) {
        if (binding.keys.at(i) == key) {
            for (std::size_t j = i; j + 1 < binding.keyCount; ++j) {
                binding.keys.at(j) = binding.keys.at(j + 1);
            }
            binding.keys.at(--binding.keyCount) = 0;
            found = true;
            break;
        }
    }
    for (std::size_t i = 0; i < binding.negativeKeyCount; ++i) {
        if (binding.negativeKeys.at(i) == key) {
            for (std::size_t j = i; j + 1 < binding.negativeKeyCount; ++j) {
                binding.negativeKeys.at(j) = binding.negativeKeys.at(j + 1);
            }
            binding.negativeKeys.at(--binding.negativeKeyCount) = 0;
            found = true;
            break;
        }
    }
    return found;
}

void ActionMapping::clearKeyBindings(ActionId action) {
    const auto it = _bindings.find(action);
    if (it != _bindings.end()) {
        it->second.keyCount = 0;
        it->second.keys.fill(0);
        it->second.negativeKeyCount = 0;
        it->second.negativeKeys.fill(0);
    }
}

bool ActionMapping::bindGamepadButton(ActionId action, int button) {
    if (button < 0) {
        return false;
    }
    ActionBinding& binding = getOrCreateBinding(action);
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

void ActionMapping::bindGamepadAxis(ActionId action, int axis, float scale) {
    ActionBinding& binding = getOrCreateBinding(action);
    binding.padAxis = axis;
    binding.padAxisDirection = scale;
}

void ActionMapping::bindAxisComposite(ActionId axis, int negativeKey, int positiveKey) {
    ActionBinding& binding = getOrCreateBinding(axis);
    binding.type = ActionType::Axis;
    if (negativeKey > 0 && binding.negativeKeyCount < MAX_KEY_BINDINGS) {
        binding.negativeKeys.at(binding.negativeKeyCount++) = negativeKey;
    }
    if (positiveKey > 0 && binding.keyCount < MAX_KEY_BINDINGS) {
        binding.keys.at(binding.keyCount++) = positiveKey;
    }
}

bool ActionMapping::isActionDown(ActionId action, int gamepadIndex) const {
    if (!IsWindowReady()) {
        return false;
    }
    const ActionBinding* binding = getBinding(action);
    if (binding == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < binding->keyCount; ++i) {
        if (IsKeyDown(binding->keys.at(i))) {
            return true;
        }
    }
    if (IsGamepadAvailable(gamepadIndex)) {
        for (std::size_t i = 0; i < binding->padButtonCount; ++i) {
            if (IsGamepadButtonDown(gamepadIndex, binding->padButtons.at(i))) {
                return true;
            }
        }
        if (binding->padAxis >= 0) {
            const float val = GetGamepadAxisMovement(gamepadIndex, binding->padAxis);
            if (binding->padAxisDirection > 0.0F && val > _deadzone) {
                return true;
            }
            if (binding->padAxisDirection < 0.0F && val < -_deadzone) {
                return true;
            }
        }
    }
    return false;
}

float ActionMapping::getAxis(ActionId action, int gamepadIndex) const {
    if (!IsWindowReady()) {
        return 0.0F;
    }
    const ActionBinding* binding = getBinding(action);
    if (binding == nullptr) {
        return 0.0F;
    }
    float axisValue = sampleKeyboardComposite(*binding);
    if (IsGamepadAvailable(gamepadIndex)) {
        axisValue += sampleGamepadButtonComposite(*binding, gamepadIndex);
        axisValue += sampleGamepadStick(*binding, gamepadIndex, _deadzone);
    }
    return std::clamp(axisValue, -1.0F, 1.0F);
}

}  // namespace input
