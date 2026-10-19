#pragma once

namespace client {

/**
 * @brief Component attached to the player's ship entity for control and movement speed.
 */
struct ShipInput {
    /** @brief Logical player index assigned to this ship. */
    int playerIndex = 0;

    /** @brief Whether player control is enabled. */
    bool enabled = true;

    /** @brief Horizontal movement speed in pixels per second. */
    float speed = 350.0F;
};

}  // namespace client
