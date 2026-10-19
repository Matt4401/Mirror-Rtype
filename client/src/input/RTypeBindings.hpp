#pragma once

#include "input/ActionMapping.hpp"

namespace client::input {

/**
 * @brief Configures default key and gamepad bindings for the R-Type game.
 *
 * Sets up accessible controls supporting AZERTY, QWERTY, arrow keys,
 * and gamepad thumbsticks/buttons.
 *
 * @param mapping Target action mapping instance to populate.
 */
void setupDefaultBindings(::input::ActionMapping& mapping);

}  // namespace client::input
