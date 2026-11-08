/**
 * @file Targeting.hpp
 * @brief How R-Type enemies aim: the nearest ship they can hit, and the missiles they fire at it.
 */

#pragma once

#include <optional>

#include "ecs/Registry.hpp"

namespace rtype::server::gameplay {

/** @brief A point aimed at, in pixels. */
struct AimPoint {
    /** @brief Horizontal position. */
    float x = 0.0F;
    /** @brief Vertical position. */
    float y = 0.0F;
};

/** @brief A velocity, in pixels per second. */
struct Velocity {
    /** @brief Horizontal speed. */
    float x = 0.0F;
    /** @brief Vertical speed. */
    float y = 0.0F;
};

/**
 * @brief Finds the nearest player ship that can be hit: replicated as a ship, and not respawning.
 * @param registry Room registry.
 * @param x Horizontal position of the shooter.
 * @param y Vertical position of the shooter.
 * @return The position of the ship, or nothing without any.
 */
[[nodiscard]] std::optional<AimPoint> nearestShip(const ecs::Registry& registry, float x, float y);

/**
 * @brief Gives the velocity of a shot from (x, y) towards a target.
 * @param x Horizontal position of the shooter.
 * @param y Vertical position of the shooter.
 * @param target Point aimed at.
 * @param speed Speed of the shot.
 * @return The velocity; straight to the left when the target is on the shooter.
 */
[[nodiscard]] Velocity aimAt(float x, float y, AimPoint target, float speed) noexcept;

/**
 * @brief Spawns a replicated enemy missile.
 * @param registry Room registry.
 * @param x Horizontal position.
 * @param y Vertical position.
 * @param velocity Velocity of the missile.
 */
void spawnEnemyMissile(ecs::Registry& registry, float x, float y, Velocity velocity);

}  // namespace rtype::server::gameplay
