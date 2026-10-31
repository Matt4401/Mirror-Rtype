#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/services/InputCodes.hpp"
#include "input/ActionId.hpp"
#include "input/InputConstants.hpp"

namespace engine {
class IWindowService;
}

namespace input {

/**
 * @brief Bindings configured for a single semantic action.
 */
struct ActionBinding {
    /** @brief Number of active key bindings in the keys array. */
    std::size_t keyCount = 0;

    /** @brief Number of active negative key bindings. */
    std::size_t negativeKeyCount = 0;

    /** @brief Number of active gamepad button bindings. */
    std::size_t padButtonCount = 0;

    /** @brief Number of active negative gamepad button bindings. */
    std::size_t negativePadButtonCount = 0;

    /** @brief Number of active mouse button bindings. */
    std::size_t mouseButtonCount = 0;

    /** @brief Human-readable name of the action. */
    std::string name;

    /** @brief Required direction (-1.0F or +1.0F) or scale multiplier for the axis. */
    float padAxisDirection = 1.0F;

    /** @brief Category of this action (Button or Axis). */
    ActionType type = ActionType::Button;

    /** @brief Gamepad analog axis bound to this action (GamepadAxis::None if none). */
    engine::GamepadAxis padAxis = engine::GamepadAxis::None;

    /** @brief Array of gamepad button codes bound to this action or positive axis contribution. */
    std::array<engine::GamepadButton, MAX_PAD_BUTTON_BINDINGS> padButtons = {};

    /** @brief Array of negative gamepad button codes for composite axes. */
    std::array<engine::GamepadButton, MAX_PAD_BUTTON_BINDINGS> negativePadButtons = {};

    /** @brief Array of mouse buttons bound to this action. Only Button actions sample them. */
    std::array<engine::MouseButton, MAX_MOUSE_BUTTON_BINDINGS> mouseButtons = {};

    /** @brief Array of keyboard key codes triggering this action or positive axis contribution. */
    std::array<engine::Key, MAX_KEY_BINDINGS> keys = {};

    /** @brief Array of negative keyboard key codes for composite axes. */
    std::array<engine::Key, MAX_KEY_BINDINGS> negativeKeys = {};
};

/**
 * @brief Manages associations between physical hardware inputs and semantic actions.
 *
 * Provides dynamic runtime action registration and remapping supporting keyboard,
 * gamepad buttons, analog axes, and composite axes.
 */
class ActionMapping {
  public:
    /** @brief Constructs empty action mapping. */
    ActionMapping();

    /**
     * @brief Registers an action by name with its category.
     * @param name Unique semantic name (e.g. "shoot", "move_x").
     * @param type Action type (Button or Axis).
     * @return Generated ActionId for the action.
     */
    ActionId registerAction(std::string_view name, ActionType type = ActionType::Button);

    /** @brief Associates a keyboard keycode with an action. */
    bool bindKey(ActionId action, engine::Key key);

    /** @brief Associates a keyboard keycode with an action by name. */
    bool bindKey(std::string_view name, engine::Key key) { return bindKey(makeActionId(name), key); }

    /** @brief Removes a keyboard keycode from an action. */
    bool unbindKey(ActionId action, engine::Key key);

    /** @brief Removes a keyboard keycode from an action by name. */
    bool unbindKey(std::string_view name, engine::Key key) { return unbindKey(makeActionId(name), key); }

    /** @brief Clears all keyboard keycodes mapped to an action. */
    void clearKeyBindings(ActionId action);

    /** @brief Clears all keyboard keycodes mapped to an action by name. */
    void clearKeyBindings(std::string_view name) { clearKeyBindings(makeActionId(name)); }

    /** @brief Checks if a key is currently bound to an action. */
    [[nodiscard]] bool hasKey(ActionId action, engine::Key key) const;

    /** @brief Checks if a key is currently bound to an action by name. */
    [[nodiscard]] bool hasKey(std::string_view name, engine::Key key) const { return hasKey(makeActionId(name), key); }

    /** @brief Associates a gamepad button with an action. */
    bool bindGamepadButton(ActionId action, engine::GamepadButton button);

    /** @brief Associates a gamepad button with an action by name. */
    bool bindGamepadButton(std::string_view name, engine::GamepadButton button) {
        return bindGamepadButton(makeActionId(name), button);
    }

    /** @brief Associates a mouse button with an action. */
    bool bindMouseButton(ActionId action, engine::MouseButton button);

    /** @brief Associates a mouse button with an action by name. */
    bool bindMouseButton(std::string_view name, engine::MouseButton button) {
        return bindMouseButton(makeActionId(name), button);
    }

    /** @brief Associates an analog stick axis and scale with an action. */
    void bindGamepadAxis(ActionId action, engine::GamepadAxis axis, float scale = 1.0F);

    /** @brief Associates an analog stick axis and scale with an action by name. */
    void bindGamepadAxis(std::string_view name, engine::GamepadAxis axis, float scale = 1.0F) {
        bindGamepadAxis(makeActionId(name), axis, scale);
    }

    /** @brief Binds negative and positive keys to a 1D continuous axis. */
    void bindAxisComposite(ActionId axis, engine::Key negativeKey, engine::Key positiveKey);

    /** @brief Binds negative and positive keys to a 1D continuous axis by name. */
    void bindAxisComposite(std::string_view name, engine::Key negativeKey, engine::Key positiveKey) {
        bindAxisComposite(makeActionId(name), negativeKey, positiveKey);
    }

    /** @brief Sets the analog stick deadzone threshold. */
    void setDeadzone(float deadzone) noexcept { _deadzone = deadzone; }

    /** @brief Gets the analog stick deadzone threshold. */
    [[nodiscard]] float deadzone() const noexcept { return _deadzone; }

    /** @brief Clears all action bindings and resets deadzone to default. */
    void clear();

    /** @brief Tells whether an action is currently active on hardware devices. */
    [[nodiscard]] bool isActionDown(const engine::IWindowService& window, ActionId action, int gamepadIndex = 0) const;

    /** @brief Tells whether an action is currently active on hardware devices by name. */
    [[nodiscard]] bool isActionDown(const engine::IWindowService& window, std::string_view name,
                                    int gamepadIndex = 0) const {
        return isActionDown(window, makeActionId(name), gamepadIndex);
    }

    /** @brief Samples continuous 1D axis value in [-1.0F, 1.0F] from hardware devices. */
    [[nodiscard]] float getAxis(const engine::IWindowService& window, ActionId action, int gamepadIndex = 0) const;

    /** @brief Samples continuous 1D axis value in [-1.0F, 1.0F] from hardware devices by name. */
    [[nodiscard]] float getAxis(const engine::IWindowService& window, std::string_view name,
                                int gamepadIndex = 0) const {
        return getAxis(window, makeActionId(name), gamepadIndex);
    }

    /** @brief Gets internal binding configuration for an action if registered. */
    [[nodiscard]] const ActionBinding* getBinding(ActionId action) const;

    /** @brief Collection of all registered action bindings. */
    [[nodiscard]] const std::unordered_map<ActionId, ActionBinding>& bindings() const noexcept { return _bindings; }

  private:
    /** @brief Retrieves or creates a binding entry for the specified action. */
    ActionBinding& getOrCreateBinding(ActionId action);

    /** @brief Samples continuous composite keyboard contributions for an action binding. */
    [[nodiscard]] static float sampleKeyboardComposite(const engine::IWindowService& window,
                                                       const ActionBinding& binding);

    /** @brief Samples continuous composite gamepad button contributions for an action binding. */
    [[nodiscard]] static float sampleGamepadButtonComposite(const engine::IWindowService& window,
                                                            const ActionBinding& binding, int gamepadIndex);

    /** @brief Samples analog stick axis contribution for an action binding with deadzone threshold. */
    [[nodiscard]] static float sampleGamepadStick(const engine::IWindowService& window, const ActionBinding& binding,
                                                  int gamepadIndex, float deadzone);

    /** @brief Table of bindings indexed by ActionId. */
    std::unordered_map<ActionId, ActionBinding> _bindings;

    /** @brief Deadzone threshold for analog axis input. */
    float _deadzone = DEFAULT_DEADZONE;
};

}  // namespace input
