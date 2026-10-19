/**
 * @file Obstacle.hpp
 * @brief Built-in "obstacle" prefab: a static solid block of level geometry.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"

namespace prefabs {

/** @brief Name under which the obstacle prefab is registered. */
inline constexpr std::string_view OBSTACLE_PREFAB = "obstacle";

/** @brief Default half-width of an obstacle in pixels. */
inline constexpr float DEFAULT_OBSTACLE_HALF_WIDTH = 32.0F;

/** @brief Default half-height of an obstacle in pixels. */
inline constexpr float DEFAULT_OBSTACLE_HALF_HEIGHT = 32.0F;

/**
 * @brief Attaches the obstacle components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * The body is static and the collider is solid: ships and projectiles collide with it.
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param x Horizontal position of the obstacle center.
 * @param y Vertical position of the obstacle center.
 * @param halfWidth Half-width of the collision box in pixels.
 * @param halfHeight Half-height of the collision box in pixels.
 */
void buildObstacle(ecs::Entity e, ecs::Registry& registry, float x, float y, float halfWidth, float halfHeight);

/**
 * @brief Registers an obstacle prefab of the given size.
 *
 * Calling it several times with different names registers size variants (e.g. "obstacle_wide").
 *
 * @param manager Prefab manager receiving the factory.
 * @param halfWidth Half-width of the collision box in pixels.
 * @param halfHeight Half-height of the collision box in pixels.
 * @param name Name under which the prefab is registered.
 */
void registerObstaclePrefab(PrefabManager& manager, float halfWidth = DEFAULT_OBSTACLE_HALF_WIDTH,
                            float halfHeight = DEFAULT_OBSTACLE_HALF_HEIGHT, std::string_view name = OBSTACLE_PREFAB);

}  // namespace prefabs
