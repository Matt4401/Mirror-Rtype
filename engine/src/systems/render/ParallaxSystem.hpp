/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Scrolls background entities tagged with ParallaxLayer.
*/

#pragma once

#include "ecs/Registry.hpp"

namespace systems {

/**
 * @brief Moves every entity that has both Transform and ParallaxLayer.
 *
 * Each update, the entity position is advanced by its autonomous scroll speed and by a fraction
 * of the active camera's displacement (parallax factor). Wrapping is applied per-axis when
 * the wrap bounds are not equal. Both X and Y axes are supported independently.
 */
class ParallaxSystem {
  public:
    /**
     * @brief Advances all parallax entities by deltaTime seconds.
     *
     * @param registry Registry containing the scene.
     * @param deltaTime Elapsed time since the last frame in seconds.
     */
    void update(ecs::Registry& registry, float deltaTime);

  private:
    float _prevCameraX = 0.0F;
    float _prevCameraY = 0.0F;
};

}  // namespace systems
