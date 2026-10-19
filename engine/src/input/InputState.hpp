#pragma once

#include <bitset>

#include "input/InputAction.hpp"

namespace input {

/**
 * @brief Singleton ECS Resource representing the global state of player actions for the current frame.
 *
 * Uses compact 32-bit bitsets for O(1) single-cycle checks without dynamic memory allocation.
 */
class InputState {
  public:
    /** @brief Default constructor. */
    InputState() = default;

    /** @brief Tells whether an action is currently held down. */
    [[nodiscard]] bool isDown(Action action) const noexcept;

    /** @brief Tells whether an action was just pressed on the current frame. */
    [[nodiscard]] bool isJustPressed(Action action) const noexcept;

    /** @brief Tells whether an action was just released on the current frame. */
    [[nodiscard]] bool isJustReleased(Action action) const noexcept;

    /** @brief Continuous horizontal axis value in [-1.0F, 1.0F]. */
    [[nodiscard]] float axisX() const noexcept;

    /** @brief Continuous vertical axis value in [-1.0F, 1.0F]. */
    [[nodiscard]] float axisY() const noexcept;

    /** @brief Sets the held state for an action. */
    void setDown(Action action, bool down) noexcept;

    /** @brief Sets the transition state for an action pressed this frame. */
    void setJustPressed(Action action, bool pressed) noexcept;

    /** @brief Sets the transition state for an action released this frame. */
    void setJustReleased(Action action, bool released) noexcept;

    /** @brief Sets the computed normalized axes. */
    void setAxes(float x, float y) noexcept;

    /** @brief Resets single-frame edge transitions. */
    void resetFrameTransitions() noexcept;

  private:
    /** @brief Bitset of actions currently held down. */
    std::bitset<ACTION_COUNT> _held;

    /** @brief Bitset of actions pressed on this frame. */
    std::bitset<ACTION_COUNT> _justPressed;

    /** @brief Bitset of actions released on this frame. */
    std::bitset<ACTION_COUNT> _justReleased;

    /** @brief Horizontal movement axis magnitude [-1.0F, 1.0F]. */
    float _axisX = 0.0F;

    /** @brief Vertical movement axis magnitude [-1.0F, 1.0F]. */
    float _axisY = 0.0F;
};

}  // namespace input
