/**
 * @file Obstacle.hpp
 * @brief Built-in "obstacle" prefab: a static solid block of level geometry.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

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
 * Parameters: the transform keys (see `makeTransform`, `x`/`y` being the block center), `halfWidth` and
 * `halfHeight` (collision box half-size in pixels, `DEFAULT_OBSTACLE_HALF_WIDTH`/`HEIGHT` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildObstacle(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
