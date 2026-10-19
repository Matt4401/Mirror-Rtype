#pragma once

namespace ecs {

/**
 * @brief Spatial state of an entity: position, rotation and scale.
 *
 * Simulation is 2D (`x`, `y`); `z` is only a depth value used to sort sprites when rendering in 2.5D.
 * Plain data with no logic, so it is available to the headless server as well as the client.
 */
struct Transform {
    /** @brief Horizontal position in world units. */
    float x = 0.0F;

    /** @brief Vertical position in world units. */
    float y = 0.0F;

    /** @brief Depth used for draw ordering; higher values are drawn in front. */
    float z = 0.0F;

    /** @brief Rotation around the Z axis, in radians. */
    float rotation = 0.0F;

    /** @brief Horizontal scale factor (1 keeps the original size, negative values flip). */
    float scaleX = 1.0F;

    /** @brief Vertical scale factor (1 keeps the original size, negative values flip). */
    float scaleY = 1.0F;
};

}  // namespace ecs
