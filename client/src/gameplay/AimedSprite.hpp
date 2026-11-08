/**
 * @file AimedSprite.hpp
 * @brief Sprites whose frames are directions: the frame shown is the one facing the nearest player ship.
 */

#pragma once

#include "ecs/Registry.hpp"

namespace client::gameplay {

/**
 * @brief A strip of `frames` frames, each one a direction around the full turn, the shown one picked by aim.
 *
 * Frame `i` faces `firstAngle + i * 360 / frames` degrees: 0 points right, 90 down (screen coordinates), turning
 * clockwise. Set by the client instead of an `ecs::AnimationComponent` for a `directional` look.
 */
struct AimedSprite {
    /** @brief Left of the first frame, in texture pixels. */
    int startX = 0;
    /** @brief Top of the strip, in texture pixels. */
    int startY = 0;
    /** @brief Width of a frame, in texture pixels. */
    int frameWidth = 0;
    /** @brief Height of a frame, in texture pixels. */
    int frameHeight = 0;
    /** @brief Pixels from one frame to the next; 0 or less for `frameWidth`. */
    int frameStride = 0;
    /** @brief Number of directions. */
    int frames = 1;
    /** @brief Direction of the first frame, in degrees. */
    float firstAngle = 0.0F;
};

/**
 * @brief Gives the frame of an `AimedSprite` facing a direction.
 * @param sprite The sprite.
 * @param degrees Direction, in degrees (0 right, 90 down); any value, wrapped around.
 * @return The index of the nearest frame, in [0, frames).
 */
[[nodiscard]] int aimedFrame(const AimedSprite& sprite, float degrees) noexcept;

/**
 * @brief Turns every `AimedSprite` towards the nearest player ship, by setting the source rectangle of its
 * `ecs::Renderable`. A sprite keeps its frame while no ship is on screen.
 * @param registry Client registry.
 */
void aimSprites(ecs::Registry& registry);

}  // namespace client::gameplay
