/**
 * @file LevelLoader.hpp
 * @brief Reads an R-Type level file (TOML) into a Level, validating every value.
 */

#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"
#include "level/Level.hpp"

namespace rtype::server::level {

/** @brief Most events a level file may declare; the extra ones are ignored with a warning. */
inline constexpr std::size_t MAX_EVENTS = 1024;

/** @brief Most enemies a single wave may spawn. */
inline constexpr std::size_t MAX_WAVE_COUNT = 32;

/** @brief Highest scroll speed accepted, in pixels per second. */
inline constexpr float MAX_SCROLL_SPEED = 2000.0F;

/** @brief Furthest event position accepted, in scroll pixels. */
inline constexpr float MAX_LEVEL_DISTANCE = 1.0e6F;

/** @brief Longest delay accepted between two enemies of a wave, in seconds. */
inline constexpr float MAX_WAVE_INTERVAL = 10.0F;

/** @brief Scroll speed used when `level.scroll_speed` is missing, in pixels per second. */
inline constexpr float DEFAULT_SCROLL_SPEED = 60.0F;

/** @brief Spawn abscissa used when a spawn event has no `x`: just past the right edge of the 1280 px scene. */
inline constexpr float DEFAULT_SPAWN_X = 1328.0F;

/** @brief Spawn ordinate used when a spawn event has no `y`: the middle of the 720 px scene. */
inline constexpr float DEFAULT_SPAWN_Y = 360.0F;

/** @brief Furthest spawn coordinate accepted on either axis, in pixels. */
inline constexpr float MAX_SPAWN_COORDINATE = 10000.0F;

/** @brief Tells whether a prefab name can be spawned by the game. */
using PrefabFilter = std::function<bool(std::string_view)>;

/**
 * @brief Builds a level from an already parsed file.
 *
 * File layout:
 * @code{.toml}
 * [level]
 * name = "Sector 1"
 * scroll_speed = 60.0        # px/s
 *
 * [[events]]
 * at = 400.0                 # scroll distance, px
 * type = "spawn"
 * prefab = "alien_scout"
 * count = 5                  # optional, default 1
 * interval = 0.4             # optional, seconds between two enemies
 * x = 1328.0                 # optional
 * y = 200.0                  # optional
 * spacing_y = 60.0           # optional, added to y for each next enemy
 * pattern = "sine"           # any other key is passed to the prefab
 *
 * [[events]]
 * at = 2400.0
 * type = "scroll"
 * speed = 30.0
 *
 * [[events]]
 * at = 6000.0
 * type = "end"
 * @endcode
 *
 * Never fails: every invalid value is corrected or its event skipped, and reported in `warnings`. A wave is expanded
 * into one spawn event per enemy, `interval` seconds apart at the scroll speed in effect where the wave starts.
 *
 * @param file Parsed level file.
 * @param isKnownPrefab Prefabs the game can spawn; a spawn event naming another one is skipped.
 * @param warnings Receives one message per corrected value or skipped event.
 * @return The level.
 */
[[nodiscard]] Level buildLevel(const config::ConfigFile& file, const PrefabFilter& isKnownPrefab,
                               std::vector<std::string>& warnings);

/**
 * @brief Reads and builds a level file.
 * @param path Level file.
 * @param isKnownPrefab Prefabs the game can spawn.
 * @param warnings Receives one message per corrected value or skipped event.
 * @return The level, or why the file could not be read or parsed.
 */
[[nodiscard]] std::expected<Level, std::string> loadLevel(const std::filesystem::path& path,
                                                          const PrefabFilter& isKnownPrefab,
                                                          std::vector<std::string>& warnings);

}  // namespace rtype::server::level
