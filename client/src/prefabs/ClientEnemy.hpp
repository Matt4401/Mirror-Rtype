/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client-side Bydos enemy prefab: animated sprite flying left, hittable by player shots.
*/

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace client::prefabs {

/** @brief Name under which the client enemy prefab is registered. */
inline constexpr std::string_view CLIENT_ENEMY_PREFAB = "client_enemy";

/** @brief Default horizontal speed of a client enemy in pixels per second (it flies to the left). */
inline constexpr float DEFAULT_CLIENT_ENEMY_SPEED = 150.0F;

/** @brief Tag that identifies an entity as a client-side enemy. */
struct ClientEnemy {};

/**
 * @brief Attaches the client enemy components to an entity.
 *
 * Sets up Transform, Renderable, AnimationComponent, a Kinematic RigidBody2D moving left, an `ENEMY` sensor
 * Collider2D and the ClientEnemy tag. Accepted parameters: transform keys (x, y, scaleX, scaleY), `speed`
 * (pixels/sec, positive values move left) and `textureId` (integer texture identifier set as a registration default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildClientEnemy(ecs::Entity e, ecs::Registry& registry, ::prefabs::PrefabParams const& params);

}  // namespace client::prefabs
