#pragma once

namespace ecs {

/**
 * @brief Component attached to entities controlled by player inputs.
 *
 * Holds the continuous movement intent vector, action flags, and movement speed.
 */
struct InputReceiver {
    int playerIndex = 0;
    bool enabled = true;
    float moveX = 0.0F;
    float moveY = 0.0F;
    bool shooting = false;
    bool charging = false;
    float speed = 350.0F;
};

}  // namespace ecs
