/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Frame-based spritesheet animation component.
*/

#pragma once

namespace ecs {

/**
 * @brief Drives frame-based animation by updating the Renderable::sourceRect each tick.
 *
 * AnimationSystem reads this component and writes the correct source rectangle into the
 * sibling Renderable component. Time advances via delta-time so it is framerate-independent.
 */
struct AnimationComponent {
    /** @brief Width of a single animation frame in the spritesheet (pixels). */
    int frameWidth = 0;

    /** @brief Height of a single animation frame in the spritesheet (pixels). */
    int frameHeight = 0;

    /** @brief Number of frames in the animation strip. */
    int frameCount = 1;

    /** @brief X pixel offset of the first frame in the spritesheet. */
    int startX = 0;

    /** @brief Y pixel offset of the first frame in the spritesheet. */
    int startY = 0;

    /** @brief Seconds each frame is displayed before advancing. */
    float frameDuration = 0.1F;

    /** @brief Accumulated time since the last frame advance (seconds). */
    float elapsed = 0.0F;

    /** @brief Current frame index in [0, frameCount). */
    int currentFrame = 0;

    /** @brief Whether the animation restarts after the last frame. */
    bool loop = true;
};

}  // namespace ecs
