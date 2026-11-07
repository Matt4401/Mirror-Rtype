#pragma once

#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"

namespace client::input {

/** @brief Button action that toggles the ready state in the lobby; it is never sent as a game input. */
inline constexpr ::input::ActionId ACTION_READY = ::input::makeActionId("ready");

/**
 * @brief Configures standard key and gamepad bindings for the R-Type game.
 *
 * Utilizes physical spatial cluster keys (Key::W, Key::A, Key::S, Key::D)
 * which naturally align with ZQSD on AZERTY keyboards and WASD on QWERTY keyboards,
 * completely avoiding cross-layout key conflicts.
 *
 * @param mapping Target action mapping instance to populate.
 */
void setupDefaultBindings(::input::ActionMapping& mapping);

}  // namespace client::input
