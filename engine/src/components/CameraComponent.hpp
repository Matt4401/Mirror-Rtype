#pragma once

namespace ecs {

/**
 * @brief Plain camera state consumed by the rendering system.
 *
 * The target is the world point viewed by the camera and the offset is its screen-space position.
 */
struct CameraComponent {
    float targetX = 0.0F;
    float targetY = 0.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    float rotation = 0.0F;
    float zoom = 1.0F;
    bool active = true;
};

}  // namespace ecs
