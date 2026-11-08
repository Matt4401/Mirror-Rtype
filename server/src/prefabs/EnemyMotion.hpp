/**
 * @file EnemyMotion.hpp
 * @brief Movement patterns and loot of R-Type enemies, read by the gameplay without a script.
 */

#pragma once

namespace prefabs {

/**
 * @brief Moves an enemy up and down along a sine wave around the height it appeared at.
 *
 * Read by `gameplay::EnemyMotionPlugin`, which writes the height of the enemy on each fixed step; the horizontal move
 * stays its velocity.
 */
struct SineWave {
    /** @brief Half-height of the wave, in pixels. */
    float amplitude = 80.0F;
    /** @brief Waves per second. */
    float frequency = 0.5F;
    /** @brief Height the wave is centered on. */
    float baseY = 0.0F;
    /** @brief Seconds since the enemy appeared. */
    float time = 0.0F;
};

/** @brief The enemy drops a power-up where it dies, when a player kills it. Read by `gameplay::CombatPlugin`. */
struct DropsPowerUp {};

}  // namespace prefabs
