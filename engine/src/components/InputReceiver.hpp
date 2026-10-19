#pragma once

namespace ecs {

/**
 * @brief Component attached to entities controlled by player inputs.
 *
 * Holds the continuous movement intent vector, action flags, and movement speed.
 */
struct InputReceiver {
    /** @brief Index of the controlling player. */
    int playerIndex = 0;

    /** @brief Whether input handling is currently enabled for this entity. */
    bool enabled = true;

    /** @brief Horizontal movement axis value (-1.0F to 1.0F). */
    float moveX = 0.0F;

    /** @brief Vertical movement axis value (-1.0F to 1.0F). */
    float moveY = 0.0F;

    /** @brief Flag indicating whether the primary shoot action is active. */
    bool shooting = false;

    /** @brief Flag indicating whether a charged attack is being held. */
    bool charging = false;

    /** @brief Base movement speed in pixels per second. */
    float speed = 350.0F;
};

}  // namespace ecs
