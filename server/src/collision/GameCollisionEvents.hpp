/**
 * @file GameCollisionEvents.hpp
 * @brief R-Type collision events, published by `CollisionRouter` from the engine contact events.
 */

#pragma once

#include <cstdint>

#include "ecs/Entity.hpp"

namespace rtype::server::collision {

/** @brief A projectile started touching something it can hit (enemy, player or obstacle). */
struct ShotHitEvent {
    /** @brief Projectile entity. */
    ecs::Entity shot = 0;

    /** @brief Entity hit by the projectile. */
    ecs::Entity target = 0;

    /** @brief Collision category of the target (`prefabs::collision::ENEMY`, `PLAYER` or `OBSTACLE`). */
    std::uint16_t targetCategory = 0;

    /** @brief True for a player shot, false for an enemy shot. */
    bool byPlayer = false;
};

/** @brief A player ship started touching an enemy ship or an obstacle. */
struct ShipContactEvent {
    /** @brief Player ship entity. */
    ecs::Entity ship = 0;

    /** @brief Enemy or obstacle entity. */
    ecs::Entity other = 0;

    /** @brief Collision category of `other` (`prefabs::collision::ENEMY` or `OBSTACLE`). */
    std::uint16_t otherCategory = 0;
};

/** @brief A player ship started touching a power-up. */
struct PowerUpTouchedEvent {
    /** @brief Player ship entity. */
    ecs::Entity ship = 0;

    /** @brief Power-up entity. */
    ecs::Entity powerUp = 0;
};

}  // namespace rtype::server::collision
