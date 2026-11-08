/*
** EPITECH PROJECT, 2026
** Squacaron
** File description:
** Frame-based spritesheet animation component.
*/

#pragma once

#include <vector>

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

    /**
     * @brief Pixels from the left of a frame to the left of the next one; 0 or less means `frameWidth` (frames that
     * touch). Larger than `frameWidth` for spritesheets with empty space between their frames.
     */
    int frameStride = 0;

    /**
     * @brief Widths of the frames of a strip whose frames differ, side by side from `startX`; empty when every frame
     * is `frameWidth` wide. When set, it gives the number of frames instead of `frameCount`, and `frameStride` is not
     * used.
     */
    std::vector<int> frameWidths;

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
