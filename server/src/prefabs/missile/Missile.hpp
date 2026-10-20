/**
 * @file Missile.hpp
 * @brief Built-in "missile" prefab: a fast projectile flying to the left.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the missile prefab is registered. */
inline constexpr std::string_view MISSILE_PREFAB = "missile";

/** @brief Default horizontal speed of a missile in pixels per second (negative: towards the player side). */
inline constexpr float DEFAULT_MISSILE_SPEED = -600.0F;

/**
 * @brief Attaches the missile components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * Parameters: the transform keys (see `makeTransform`) and `speed` (horizontal velocity in pixels per second,
 * `DEFAULT_MISSILE_SPEED` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildMissile(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
