/**
 * @file AutoFire.hpp
 * @brief Latch behind the auto-fire accessibility option: one press starts firing, the next one stops it.
 */

#pragma once

namespace client {

/**
 * @brief Turns the shoot button into an on/off switch, so a player does not have to hold or mash it.
 *
 * Fed once per fixed step with the raw button state; the edge is detected here, so it does not depend on how many
 * fixed steps run in a frame.
 */
class AutoFireLatch {
  public:
    /**
     * @brief Advances the latch by one step.
     * @param held Whether the shoot button is held during this step.
     * @return Whether the ship fires during this step.
     */
    bool update(bool held) noexcept {
        if (held && !_wasHeld) {
            _firing = !_firing;
        }
        _wasHeld = held;
        return _firing;
    }

    /** @brief Stops firing and forgets the button state, e.g. when a game starts or the option changes. */
    void reset() noexcept {
        _firing = false;
        _wasHeld = false;
    }

    /**
     * @brief Tells whether the latch is on.
     * @return Value returned by the last `update`.
     */
    [[nodiscard]] bool firing() const noexcept { return _firing; }

  private:
    /** @brief Whether the ship is firing. */
    bool _firing = false;
    /** @brief Button state at the previous step, to detect presses. */
    bool _wasHeld = false;
};

}  // namespace client
