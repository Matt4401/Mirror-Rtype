#pragma once

#include <string_view>
#include <unordered_map>

#include "input/ActionId.hpp"

namespace input {

/**
 * @brief State values associated with a single semantic action.
 */
struct ActionData {
    /** @brief Whether action is currently held down. */
    bool held = false;
    /** @brief Whether action was pressed during this frame. */
    bool justPressed = false;
    /** @brief Whether action was released during this frame. */
    bool justReleased = false;
    /** @brief Continuous magnitude for 1D axis in [-1.0F, 1.0F]. */
    float axis = 0.0F;
};

/**
 * @brief 2D directional axis vector sampled from two orthogonal 1D axes.
 */
struct AxisVector2 {
    /** @brief Horizontal component in [-1.0F, 1.0F]. */
    float x = 0.0F;
    /** @brief Vertical component in [-1.0F, 1.0F]. */
    float y = 0.0F;
};

/**
 * @brief Mouse cursor and wheel state sampled once per frame.
 */
struct MouseState {
    /** @brief Horizontal cursor position in window pixels. */
    float x = 0.0F;
    /** @brief Vertical cursor position in window pixels. */
    float y = 0.0F;
    /** @brief Wheel movement of the current frame, positive when scrolling up. */
    float wheel = 0.0F;
};

/**
 * @brief ECS component holding dynamic action states and continuous axes for an entity.
 */
struct InputState {
    /** @brief Logical player index owning this input state component. */
    int playerIndex = 0;

    /** @brief Table of action states and magnitudes indexed by action ID. */
    std::unordered_map<ActionId, ActionData> actions;

    /** @brief Mouse cursor and wheel state. */
    MouseState mouse;

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
     * @brief Returns a 2D composite directional vector with optional length clamping.
     * @param axisX Target horizontal action ID.
     * @param axisY Target vertical action ID.
     * @param normalize If true, clamps diagonal vector length to 1.0F to avoid excessive diagonal speed.
     * @return 2D directional vector.
     */
    [[nodiscard]] AxisVector2 getVector2(ActionId axisX, ActionId axisY, bool normalize = true) const noexcept;

    /**
     * @brief Returns a 2D composite directional vector with optional length clamping by name.
     * @param axisX Target horizontal action name.
     * @param axisY Target vertical action name.
     * @param normalize If true, clamps diagonal vector length to 1.0F to avoid excessive diagonal speed.
     * @return 2D directional vector.
     */
    [[nodiscard]] AxisVector2 getVector2(std::string_view axisX, std::string_view axisY,
                                         bool normalize = true) const noexcept {
        return getVector2(makeActionId(axisX), makeActionId(axisY), normalize);
    }

    /** @brief Sets the held state for an action. */
    void setDown(ActionId action, bool down);
    /** @brief Sets the held state for an action by name. */
    void setDown(std::string_view name, bool down) { setDown(makeActionId(name), down); }

    /** @brief Sets the transition state for an action pressed this frame. */
    void setJustPressed(ActionId action, bool pressed);
    /** @brief Sets the transition state for an action pressed this frame by name. */
    void setJustPressed(std::string_view name, bool pressed) { setJustPressed(makeActionId(name), pressed); }

    /** @brief Sets the transition state for an action released this frame. */
    void setJustReleased(ActionId action, bool released);
    /** @brief Sets the transition state for an action released this frame by name. */
    void setJustReleased(std::string_view name, bool released) { setJustReleased(makeActionId(name), released); }

    /** @brief Sets the continuous value for an axis. */
    void setAxis(ActionId action, float value);
    /** @brief Sets the continuous value for an axis by name. */
    void setAxis(std::string_view name, float value) { setAxis(makeActionId(name), value); }

    /** @brief Resets single-frame edge transitions. */
    void resetFrameTransitions() noexcept;

    /** @brief Clears all action states and axis values. */
    void clear() noexcept;
};

}  // namespace input
