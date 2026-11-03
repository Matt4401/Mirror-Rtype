/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Advances spritesheet animations and writes the current frame into Renderable.
*/

#pragma once

#include "ecs/Registry.hpp"

namespace systems {

/**
 * @brief Updates every entity that has AnimationComponent + Renderable.
 *
 * The system accumulates delta-time, advances currentFrame when elapsed exceeds
 * frameDuration, and writes the matching source rectangle into the Renderable so
 * the RenderSystem draws the correct frame without needing to know about animations.
 */
class AnimationSystem {
  public:
    /**
     * @brief Advances all animations by deltaTime seconds.
     *
     * @param registry Registry containing the scene.
     * @param deltaTime Elapsed time since the last frame in seconds.
     */
    static void update(ecs::Registry& registry, float deltaTime);
};

}  // namespace systems
