#pragma once

#include <string_view>
#include <unordered_map>

#include "input/ActionId.hpp"

namespace input {

/**
 * @brief Singleton ECS Resource representing the global state of dynamic actions and axes for the current frame.
 */
class InputState {
  public:
    /** @brief Default constructor. */
    InputState() = default;

    /**
     * @brief Tells whether an action is currently held down.
     * @param action Target action ID.
     * @return True if action is down, false otherwise.
     */
    [[nodiscard]] bool isDown(ActionId action) const noexcept;

    /**
     * @brief Tells whether an action is currently held down by name.
     * @param name Target action name.
     * @return True if action is down, false otherwise.
     */
    [[nodiscard]] bool isDown(std::string_view name) const noexcept { return isDown(makeActionId(name)); }

    /**
     * @brief Tells whether an action was just pressed on the current frame.
     * @param action Target action ID.
     * @return True if just pressed, false otherwise.
     */
    [[nodiscard]] bool isJustPressed(ActionId action) const noexcept;

    /**
     * @brief Tells whether an action was just pressed on the current frame by name.
     * @param name Target action name.
     * @return True if just pressed, false otherwise.
     */
    [[nodiscard]] bool isJustPressed(std::string_view name) const noexcept { return isJustPressed(makeActionId(name)); }

    /**
     * @brief Tells whether an action was just released on the current frame.
     * @param action Target action ID.
     * @return True if just released, false otherwise.
     */
    [[nodiscard]] bool isJustReleased(ActionId action) const noexcept;

    /**
     * @brief Tells whether an action was just released on the current frame by name.
     * @param name Target action name.
     * @return True if just released, false otherwise.
     */
    [[nodiscard]] bool isJustReleased(std::string_view name) const noexcept {
        return isJustReleased(makeActionId(name));
    }

    /**
     * @brief Returns the continuous axis value in [-1.0F, 1.0F].
     * @param action Target action ID.
     * @return Normalized axis magnitude.
     */
    [[nodiscard]] float getAxis(ActionId action) const noexcept;

    /**
     * @brief Returns the continuous axis value in [-1.0F, 1.0F] by name.
     * @param name Target action name.
     * @return Normalized axis magnitude.
     */
    [[nodiscard]] float getAxis(std::string_view name) const noexcept { return getAxis(makeActionId(name)); }

    /**
     * @brief Sets the held state for an action.
     * @param action Target action ID.
     * @param down Whether action is down.
     */
    void setDown(ActionId action, bool down);

    /**
     * @brief Sets the held state for an action by name.
     * @param name Target action name.
     * @param down Whether action is down.
     */
    void setDown(std::string_view name, bool down) { setDown(makeActionId(name), down); }

    /**
     * @brief Sets the transition state for an action pressed this frame.
     * @param action Target action ID.
     * @param pressed Whether action was pressed.
     */
    void setJustPressed(ActionId action, bool pressed);

    /**
     * @brief Sets the transition state for an action pressed this frame by name.
     * @param name Target action name.
     * @param pressed Whether action was pressed.
     */
    void setJustPressed(std::string_view name, bool pressed) { setJustPressed(makeActionId(name), pressed); }

    /**
     * @brief Sets the transition state for an action released this frame.
     * @param action Target action ID.
     * @param released Whether action was released.
     */
    void setJustReleased(ActionId action, bool released);

    /**
     * @brief Sets the transition state for an action released this frame by name.
     * @param name Target action name.
     * @param released Whether action was released.
     */
    void setJustReleased(std::string_view name, bool released) { setJustReleased(makeActionId(name), released); }

    /**
     * @brief Sets the continuous value for an axis.
     * @param action Target action ID.
     * @param value Normalized axis magnitude.
     */
    void setAxis(ActionId action, float value);

    /**
     * @brief Sets the continuous value for an axis by name.
     * @param name Target action name.
     * @param value Normalized axis magnitude.
     */
    void setAxis(std::string_view name, float value) { setAxis(makeActionId(name), value); }

    /**
     * @brief Resets single-frame edge transitions.
     */
    void resetFrameTransitions() noexcept;

    /**
     * @brief Clears all action states and axis values.
     */
    void clear() noexcept;

  private:
    /** @brief Table of held state flags indexed by action ID. */
    std::unordered_map<ActionId, bool> _held;

    /** @brief Table of just-pressed transitions indexed by action ID. */
    std::unordered_map<ActionId, bool> _justPressed;

    /** @brief Table of just-released transitions indexed by action ID. */
    std::unordered_map<ActionId, bool> _justReleased;

    /** @brief Table of continuous axis magnitudes indexed by action ID. */
    std::unordered_map<ActionId, float> _axes;
};

}  // namespace input
