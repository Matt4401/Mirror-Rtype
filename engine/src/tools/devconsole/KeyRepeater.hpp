/**
 * @file KeyRepeater.hpp
 * @brief Turns the held state of a key into presses, with optional auto-repeat.
 */

#pragma once

namespace tools {

/**
 * @brief Turns the held state of a key into presses: one when it goes down, then one every `interval` seconds once it
 * has been held for `delay` seconds (if repeating).
 */
class KeyRepeater {
  public:
    /**
     * @brief Creates a repeater.
     * @param repeats Whether holding the key repeats the press.
     * @param delay Seconds held before the first repeat.
     * @param interval Seconds between two repeats.
     */
    KeyRepeater(bool repeats, float delay, float interval) noexcept
        : _repeats(repeats), _delay(delay), _interval(interval) {}

    /**
     * @brief Advances the key by one frame.
     * @param down Whether the key is held this frame.
     * @param dt Frame duration in seconds.
     * @return `true` if the key counts as pressed this frame.
     */
    bool update(bool down, float dt) noexcept;

    /** @brief Forgets the key state, so a key already held does not count as a new press. */
    void reset(bool down) noexcept;

  private:
    /** @brief Whether holding the key repeats the press. */
    bool _repeats;
    /** @brief Seconds held before the first repeat. */
    float _delay;
    /** @brief Seconds between two repeats. */
    float _interval;
    /** @brief Whether the key was held last frame. */
    bool _held = false;
    /** @brief Seconds left before the next repeat. */
    float _timer = 0.0F;
};

}  // namespace tools
