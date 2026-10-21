/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Marks an entity as part of a scrolling parallax background layer.
*/

#pragma once

namespace ecs {

/**
 * @brief Scrolls the entity's Transform along X each frame and wraps it around the screen width.
 *
 * Entities tagged with this component are moved by ParallaxSystem. Stars (or any background
 * sprite) should have this component alongside Transform and Renderable.
 */
struct ParallaxLayer {
    /** @brief Pixels per second the entity scrolls toward negative X (left). */
    float scrollSpeedX = 50.0F;

    /** @brief X coordinate at which the entity wraps back to the right edge. */
    float wrapMinX = -8.0F;

    /** @brief X coordinate used when re-spawning the entity after a wrap. */
    float wrapMaxX = 1280.0F;
};

}  // namespace ecs
