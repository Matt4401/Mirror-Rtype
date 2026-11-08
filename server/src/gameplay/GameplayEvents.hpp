/**
 * @file GameplayEvents.hpp
 * @brief Events the R-Type gameplay plugins publish on the room's `EventBus` to talk to each other.
 */

#pragma once

#include <cstdint>

#include "ecs/Entity.hpp"
#include "timeline/TimelineEvent.hpp"

namespace rtype::server::gameplay {

/**
 * @brief The level director plays a level event: now, or once the scroll resumes if it was held.
 *
 * Each plugin handles the types it knows: `terrain` for the terrain, `boss` for the boss fights.
 */
struct LevelEventPlayed {
    /** @brief The event, as the level loader built it. */
    timeline::TimelineEvent event;
};

/** @brief The level starts, or restarts after its `end` event. */
struct LevelStarted {};

/**
 * @brief The level reached its `end` event during a game (after `LevelDirectorPlugin::startGame()`).
 *
 * The plugin keeping the scores turns it into the protocol messages the room sends: `protocol::LevelComplete`, then
 * `protocol::GameOver` after the last level.
 */
struct LevelFinished {
    /** @brief Index of the finished level. */
    std::uint8_t levelIndex = 0;
    /** @brief True for the last level of the game: the game is won. */
    bool lastLevel = true;
};

/** @brief A boss was spawned: the scroll waits until the fight is over. Published for each boss. */
struct BossFightStarted {};

/** @brief No boss is left: the fight is over. */
struct BossFightEnded {};

/** @brief A ship was hit by an enemy, an enemy shot or the terrain. */
struct ShipDestroyed {
    /** @brief Player ship. */
    ecs::Entity ship = 0;
};

/** @brief A destroyed ship came back at its start position: it blinks, and the scroll waits until it is over. */
struct ShipRespawnStarted {
    /** @brief Player ship. */
    ecs::Entity ship = 0;
};

/** @brief The last respawning ship can be hit again: no respawn holds the scroll any more. */
struct AllRespawnsOver {};

}  // namespace rtype::server::gameplay
