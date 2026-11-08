/**
 * @file GameplayComponents.hpp
 * @brief Scene constants and components shared by the R-Type gameplay plugins.
 */

#pragma once

#include <cstdint>

#include "game/ReplicationCodec.hpp"

namespace rtype::server::gameplay {

/** @brief Width of the scene in world units (pixels). */
inline constexpr float SCENE_WIDTH = 1280.0F;
/** @brief Height of the scene in world units (pixels). */
inline constexpr float SCENE_HEIGHT = 720.0F;
/** @brief Margin past the screen edges after which bullets and enemies are destroyed. */
inline constexpr float OFFSCREEN_MARGIN = 64.0F;

/** @brief A ship that just respawned: it cannot be hit until `secondsLeft` reaches 0. Set by `RespawnPlugin`. */
struct Respawning {
    /** @brief Seconds before the ship can be hit again. */
    float secondsLeft = 0.0F;
};

/** @brief Lives left of a player ship; a ship without it respawns forever. Set by `giveLives()`. */
struct Lives {
    /** @brief Lives left, the current one included. */
    std::uint8_t left = 0;
};

/** @brief A player ship that lost its last life: out of the game, it cannot be hit, move or shoot any more. */
struct Eliminated {};

/** @brief Marks a terrain block, built by `TerrainPlugin` from a level `terrain` event. */
struct TerrainBlock {
    /** @brief Half-width of the block. */
    float halfWidth = game::TERRAIN_BLOCK_WIDTH / 2.0F;
    /** @brief Half-height of the block. */
    float halfHeight = 0.0F;
};

}  // namespace rtype::server::gameplay
