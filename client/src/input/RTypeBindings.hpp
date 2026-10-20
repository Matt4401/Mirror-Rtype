#pragma once

#include "input/ActionMapping.hpp"

namespace client::input {

/**
 * @brief Configures standard key and gamepad bindings for the R-Type game.
 *
 * Utilizes Raylib/GLFW physical spatial cluster keys (KEY_W, KEY_A, KEY_S, KEY_D)
 * which naturally align with ZQSD on AZERTY keyboards and WASD on QWERTY keyboards,
 * completely avoiding cross-layout key conflicts.
 *
 * @param mapping Target action mapping instance to populate.
 */
void setupDefaultBindings(::input::ActionMapping& mapping);

}  // namespace client::input
