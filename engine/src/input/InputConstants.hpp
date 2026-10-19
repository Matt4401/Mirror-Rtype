#pragma once

#include <cstddef>

namespace input {

/** @brief Maximum number of alternative keyboard keycodes mapped to a single action. */
inline constexpr std::size_t MAX_KEY_BINDINGS = 4;

/** @brief Maximum number of gamepad buttons mapped to a single action. */
inline constexpr std::size_t MAX_PAD_BUTTON_BINDINGS = 2;

/** @brief Default analog stick deadzone to filter hardware noise. */
inline constexpr float DEFAULT_DEADZONE = 0.2F;

}  // namespace input
