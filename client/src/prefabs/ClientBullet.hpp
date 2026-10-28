/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client-side player bullet prefab: animated sprite driven by the physics system.
*/

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace client::prefabs {

/** @brief Name under which the client bullet prefab is registered. */
inline constexpr std::string_view CLIENT_BULLET_PREFAB = "client_bullet";

/** @brief Default horizontal speed of a client bullet in pixels per second. */
inline constexpr float DEFAULT_CLIENT_BULLET_SPEED = 700.0F;

/** @brief Tag that identifies an entity as a client-side player bullet. */
struct ClientBullet {};

/**
 * @brief Attaches the client bullet components to an entity.
 *
 * Sets up Transform, Renderable, AnimationComponent, RigidBody2D and the ClientBullet tag.
 * Accepted parameters: transform keys (x, y, scaleX, scaleY), `speed` (horizontal pixels/sec),
 * and `textureId` (integer texture identifier set as a registration default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildClientBullet(ecs::Entity e, ecs::Registry& registry, ::prefabs::PrefabParams const& params);

}  // namespace client::prefabs
