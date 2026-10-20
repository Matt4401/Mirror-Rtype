/**
 * @file AlienScout.hpp
 * @brief Built-in "alien_scout" prefab: a basic enemy flying straight to the left.
 *
 * The scout has no AI yet. Once enemy scripts exist, attach a `ScriptComponent` in `buildAlienScout`.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the alien scout prefab is registered. */
inline constexpr std::string_view ALIEN_SCOUT_PREFAB = "alien_scout";

/** @brief Default horizontal speed of an alien scout in pixels per second (negative: towards the player side). */
inline constexpr float DEFAULT_ALIEN_SCOUT_SPEED = -150.0F;

/** @brief Half-size of the alien scout hitbox in pixels. */
inline constexpr float ALIEN_SCOUT_HALF_SIZE = 16.0F;

/**
 * @brief Attaches the alien scout components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * The collider belongs to the `ENEMY` category: it hits the player and is hit by player shots.
 *
 * Parameters: the transform keys (see `makeTransform`) and `speed` (horizontal velocity in pixels per second,
 * `DEFAULT_ALIEN_SCOUT_SPEED` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildAlienScout(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
