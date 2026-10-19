#pragma once

namespace ecs {

/**
 * @brief Plain camera state consumed by the rendering system.
 *
 * The target is the world point viewed by the camera and the offset is its screen-space position.
 */
struct CameraComponent {
    /** @brief World coordinate X targeted by the camera center. */
    float targetX = 0.0F;

    /** @brief World coordinate Y targeted by the camera center. */
    float targetY = 0.0F;

    /** @brief Screen offset X in pixels from the top-left screen origin. */
    float offsetX = 0.0F;

    /** @brief Screen offset Y in pixels from the top-left screen origin. */
    float offsetY = 0.0F;

    /** @brief Camera roll rotation angle in degrees. */
    float rotation = 0.0F;

    /** @brief Camera zoom scale factor (1.0F is neutral 1:1 scale). */
    float zoom = 1.0F;

    /** @brief Flag indicating whether this camera is actively rendering the scene. */
    bool active = true;
};

}  // namespace ecs
