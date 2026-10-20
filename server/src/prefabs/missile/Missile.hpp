/**
 * @file Missile.hpp
 * @brief Built-in "missile" prefab: a fast projectile flying to the left.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"

namespace prefabs {

/** @brief Name under which the missile prefab is registered. */
inline constexpr std::string_view MISSILE_PREFAB = "missile";

/** @brief Default horizontal speed of a missile in pixels per second (negative: towards the player side). */
inline constexpr float DEFAULT_MISSILE_SPEED = -600.0F;

/**
 * @brief Attaches the missile components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param x Initial horizontal position.
 * @param y Initial vertical position.
 * @param speed Horizontal velocity in pixels per second.
 */
void buildMissile(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed);

/**
 * @brief Registers a missile prefab flying at the given speed.
 *
 * Calling it several times with different names registers missile variants (e.g. "fast_missile").
 *
 * @param manager Prefab manager receiving the factory.
 * @param speed Horizontal velocity in pixels per second.
 * @param name Name under which the prefab is registered.
 */
void registerMissilePrefab(PrefabManager& manager, float speed = DEFAULT_MISSILE_SPEED,
                           std::string_view name = MISSILE_PREFAB);

}  // namespace prefabs
