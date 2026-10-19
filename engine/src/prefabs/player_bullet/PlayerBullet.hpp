/**
 * @file PlayerBullet.hpp
 * @brief Built-in "player_bullet" prefab: a fast projectile fired by the player, flying to the right.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"

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
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param x Initial horizontal position.
 * @param y Initial vertical position.
 * @param speed Horizontal velocity in pixels per second.
 */
void buildPlayerBullet(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed);

/**
 * @brief Registers a player bullet prefab flying at the given speed.
 *
 * @param manager Prefab manager receiving the factory.
 * @param speed Horizontal velocity in pixels per second.
 * @param name Name under which the prefab is registered.
 */
void registerPlayerBulletPrefab(PrefabManager& manager, float speed = DEFAULT_PLAYER_BULLET_SPEED,
                                std::string_view name = PLAYER_BULLET_PREFAB);

}  // namespace prefabs
