/**
 * @file FaceMotion.hpp
 * @brief Sprites that turn to face where they move, such as shots flying in any direction.
 */

#pragma once

#include "ecs/Registry.hpp"

namespace client::gameplay {

/** @brief Smallest move in one frame, in pixels, that turns the sprite: below it, the sprite keeps its direction. */
inline constexpr float MIN_FACING_MOVE = 0.01F;

/**
 * @brief Turns an entity towards its movement.
 *
 * The server only replicates positions: the direction comes from the move between two frames. Added by the client to
 * the entities of a `face_motion` look.
 */
struct FaceMotion {
    /** @brief Direction the sprite faces when not rotated, in radians: 0 for right, pi for a mirrored sprite. */
    float forward = 0.0F;
    /** @brief Horizontal position on the previous frame. */
    float lastX = 0.0F;
    /** @brief Vertical position on the previous frame. */
    float lastY = 0.0F;
    /** @brief False until the first frame, which has no previous position. */
    bool seen = false;
};

/**
 * @brief Rotates every `FaceMotion` entity towards the way it moved since the previous frame. Run after the
 * interpolation, which writes the positions, so that the direction follows what is drawn.
 * @param registry Client registry.
 */
void faceMotion(ecs::Registry& registry);

}  // namespace client::gameplay
