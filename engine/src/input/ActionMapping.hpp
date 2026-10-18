#pragma once

#include <array>
#include <cstddef>

#include "input/InputAction.hpp"
#include "input/InputConstants.hpp"

namespace input {

/**
 * @brief Bindings configured for a single semantic action.
 */
struct ActionBinding {
    std::array<int, MAX_KEY_BINDINGS> keys = {0, 0, 0, 0};
    std::size_t keyCount = 0;

    std::array<int, MAX_PAD_BUTTON_BINDINGS> padButtons = {-1, -1};
    std::size_t padButtonCount = 0;

    int padAxis = -1;
    float padAxisDirection = 0.0F;
};

/**
 * @brief Manages associations between physical hardware inputs and semantic actions.
 *
 * Provides out-of-the-box international layout support (AZERTY + QWERTY + Arrows + Gamepad)
 * and allows full runtime remapping to support accessibility guidelines.
 */
class ActionMapping {
  public:
    ActionMapping();

    /** @brief Associates a keyboard keycode with an action. */
    bool bindKey(Action action, int key);

    /** @brief Removes a keyboard keycode from an action. */
    bool unbindKey(Action action, int key);

    /** @brief Clears all keyboard keycodes mapped to an action. */
    void clearKeyBindings(Action action);

    /** @brief Checks if a key is currently bound to an action. */
    [[nodiscard]] bool hasKey(Action action, int key) const;

    /** @brief Associates a gamepad button with an action. */
    bool bindGamepadButton(Action action, int button);

    /** @brief Associates an analog stick axis and direction with an action. */
    void bindGamepadAxis(Action action, int axis, float direction);

    /** @brief Sets the analog stick deadzone threshold. */
    void setDeadzone(float deadzone) noexcept { _deadzone = deadzone; }

    /** @brief Gets the analog stick deadzone threshold. */
    [[nodiscard]] float deadzone() const noexcept { return _deadzone; }

    /** @brief Resets all mappings to default accessible configurations. */
    void resetToDefaults();

    /** @brief Tells whether an action is currently active on hardware devices. */
    [[nodiscard]] bool isActionDown(Action action, int gamepadIndex = 0) const;

    /** @brief Tells whether an action was just pressed on hardware devices. */
    [[nodiscard]] bool isActionPressed(Action action, int gamepadIndex = 0) const;

    /** @brief Tells whether an action was just released on hardware devices. */
    [[nodiscard]] bool isActionReleased(Action action, int gamepadIndex = 0) const;

    /** @brief Gets the internal binding struct for an action. */
    [[nodiscard]] const ActionBinding& getBinding(Action action) const;

  private:
    std::array<ActionBinding, ACTION_COUNT> _bindings;
    float _deadzone = DEFAULT_DEADZONE;
};

}  // namespace input
