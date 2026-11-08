/**
 * @file EnemyGun.hpp
 * @brief Gun of an R-Type enemy that fires on its own, without a script.
 */

#pragma once

namespace prefabs {

/**
 * @brief Fires missiles every `interval` seconds while the enemy is on the screen.
 *
 * Read by `gameplay::EnemyFirePlugin`, which spawns the replicated missiles.
 */
struct EnemyGun {
    /** @brief Seconds between two shots. */
    float interval = 2.0F;
    /** @brief Seconds left before the next shot. */
    float cooldown = 1.0F;
    /** @brief Speed of the missiles, in pixels per second; each shot goes for the nearest ship. */
    float shotSpeed = 300.0F;
};

}  // namespace prefabs
