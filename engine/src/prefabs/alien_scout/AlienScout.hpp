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
#include "prefabs/PrefabManager.hpp"

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
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param x Initial horizontal position.
 * @param y Initial vertical position.
 * @param speed Horizontal velocity in pixels per second.
 */
void buildAlienScout(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed);

/**
 * @brief Registers an alien scout prefab flying at the given speed.
 *
 * @param manager Prefab manager receiving the factory.
 * @param speed Horizontal velocity in pixels per second.
 * @param name Name under which the prefab is registered.
 */
void registerAlienScoutPrefab(PrefabManager& manager, float speed = DEFAULT_ALIEN_SCOUT_SPEED,
                              std::string_view name = ALIEN_SCOUT_PREFAB);

}  // namespace prefabs
