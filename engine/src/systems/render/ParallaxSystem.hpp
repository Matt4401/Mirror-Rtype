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
 * Each update, the entity's X position is decremented by scrollSpeedX * deltaTime.
 * When it falls below wrapMinX the entity is repositioned at wrapMaxX, creating
 * seamless infinite scrolling without any special background texture.
 */
class ParallaxSystem {
  public:
    /**
     * @brief Advances all parallax entities by deltaTime seconds.
     *
     * @param registry Registry containing the scene.
     * @param deltaTime Elapsed time since the last frame in seconds.
     */
    static void update(ecs::Registry& registry, float deltaTime);
};

}  // namespace systems
