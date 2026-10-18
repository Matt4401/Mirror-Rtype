#pragma once

#include <cstddef>
#include <cstdint>

namespace input {

/**
 * @brief Semantic game actions decoupled from physical hardware inputs.
 */
enum class Action : std::uint8_t {
    MoveUp = 0,
    MoveDown,
    MoveLeft,
    MoveRight,
    Shoot,
    ChargeWeapon,
    Count,
};

/** @brief Total count of distinct actions. */
inline constexpr std::size_t ACTION_COUNT = static_cast<std::size_t>(Action::Count);

/**
 * @brief Transition states for digital actions.
 */
enum class ActionState : std::uint8_t {
    Released = 0,
    Pressed = 1,
    Down = 2,
};

}  // namespace input
