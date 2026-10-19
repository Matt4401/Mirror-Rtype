#pragma once

#include <cstdint>
#include <string_view>

namespace input {

/**
 * @brief Numeric identifier for semantic input actions (computed via FNV-1a hash).
 */
using ActionId = std::uint32_t;

/**
 * @brief Computes a deterministic 32-bit FNV-1a hash for an action name string.
 * @param name Semantic name of the action (e.g. "shoot", "move_x").
 * @return 32-bit hash identifying the action.
 */
constexpr ActionId makeActionId(std::string_view name) noexcept {
    std::uint32_t hash = 2166136261U;
    for (char c : name) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 16777619U;
    }
    return hash;
}

/**
 * @brief Type category of a semantic input action.
 */
enum class ActionType : std::uint8_t {
    Button,
    Axis,
};

/**
 * @brief Transition states for digital actions.
 */
enum class ActionState : std::uint8_t {
    Released = 0,
    Pressed = 1,
    Down = 2,
};

}  // namespace input
