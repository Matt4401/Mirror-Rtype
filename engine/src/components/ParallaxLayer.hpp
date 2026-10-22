/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Marks an entity as part of a scrolling parallax background layer.
*/

#pragma once

namespace ecs {

/**
 * @brief Controls the scrolling behaviour of a background entity.
 *
 * Each frame ParallaxSystem moves the entity by the autonomous scroll speed and by a fraction
 * of the camera's displacement (parallax factor). Both axes are supported independently, and
 * wrapping is enabled per-axis when wrapMin != wrapMax.
 */
struct ParallaxLayer {
    /** @brief Autonomous scroll velocity on X in pixels/sec (positive = rightward). */
    float scrollSpeedX = 0.0F;

    /** @brief Autonomous scroll velocity on Y in pixels/sec (positive = downward). */
    float scrollSpeedY = 0.0F;

    /** @brief Fraction of camera X displacement applied to this layer (0 = ignore camera, 1 = locked). */
    float cameraFactorX = 0.0F;

    /** @brief Fraction of camera Y displacement applied to this layer (0 = ignore camera, 1 = locked). */
    float cameraFactorY = 0.0F;

    /** @brief X position below which the entity wraps to wrapMaxX. No wrap when equal to wrapMaxX. */
    float wrapMinX = 0.0F;

    /** @brief X position the entity is teleported to after an X wrap. */
    float wrapMaxX = 0.0F;

    /** @brief Y position below which the entity wraps to wrapMaxY. No wrap when equal to wrapMaxY. */
    float wrapMinY = 0.0F;

    /** @brief Y position the entity is teleported to after a Y wrap. */
    float wrapMaxY = 0.0F;
};

}  // namespace ecs
