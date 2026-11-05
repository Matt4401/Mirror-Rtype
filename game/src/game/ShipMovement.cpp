#include "game/ShipMovement.hpp"

#include <algorithm>
#include <cmath>

#include "components/InputBuffer.hpp"
#include "components/Transform.hpp"

namespace rtype::game {

void applyShipInput(ecs::Transform& transform, const ecs::NetInput& input, const float deltaTime) {
    float moveX = 0.0F;
    float moveY = 0.0F;
    for (const auto& [action, value] : input.axes) {
        if (!std::isfinite(value)) {
            continue;
        }
        if (action == ACTION_MOVE_X) {
            moveX = std::clamp(value, -1.0F, 1.0F);
        } else if (action == ACTION_MOVE_Y) {
            moveY = std::clamp(value, -1.0F, 1.0F);
        }
    }
    if (const float length = std::hypot(moveX, moveY); length > 1.0F) {
        moveX /= length;
        moveY /= length;
    }
    transform.x = std::clamp(transform.x + (moveX * SHIP_SPEED * deltaTime), SHIP_MIN_X, SHIP_MAX_X);
    transform.y = std::clamp(transform.y + (moveY * SHIP_SPEED * deltaTime), SHIP_MIN_Y, SHIP_MAX_Y);
}

}  // namespace rtype::game
