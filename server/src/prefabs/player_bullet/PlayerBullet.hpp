/**
 * @file PlayerBullet.hpp
 * @brief Built-in "player_bullet" prefab: a fast projectile fired by the player, flying to the right.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the player bullet prefab is registered. */
inline constexpr std::string_view PLAYER_BULLET_PREFAB = "player_bullet";

/** @brief Default horizontal speed of a player bullet in pixels per second (positive: towards the enemies). */
inline constexpr float DEFAULT_PLAYER_BULLET_SPEED = 900.0F;

/**
 * @brief Attaches the player bullet components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * The collider belongs to the `PLAYER_SHOT` category: it hits enemies and obstacles, never the player.
 *
 * Parameters: the transform keys (see `makeTransform`) and `speed` (horizontal velocity in pixels per second,
 * `DEFAULT_PLAYER_BULLET_SPEED` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildPlayerBullet(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
