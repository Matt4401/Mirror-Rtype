/**
 * @file LevelLoader.hpp
 * @brief Reads an R-Type level file (TOML) into a Level, validating every value.
 */

#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
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

/**
 * @brief Lowest scroll speed accepted, in pixels per second.
 *
 * The timeline cursor is the scroll distance: at 0, no later event could ever fire, so the level would be frozen for
 * good. Only the gameplay stops the scroll, during a boss fight or a respawn, and it resumes it.
 */
inline constexpr float MIN_SCROLL_SPEED = 10.0F;

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

/** @brief Most terrain sections a level file may declare; the extra ones are ignored with a warning. */
inline constexpr std::size_t MAX_TERRAIN_SECTIONS = 256;

/** @brief Height of a terrain section when `height` is missing, in pixels. */
inline constexpr float DEFAULT_TERRAIN_HEIGHT = 120.0F;

/** @brief Highest terrain section accepted, in pixels: half of the 720 px scene, so a corridor always stays open. */
inline constexpr float MAX_TERRAIN_HEIGHT = 360.0F;

/** @brief Longest boss script path accepted, in characters. */
inline constexpr std::size_t MAX_SCRIPT_PATH_LENGTH = 256;

/** @brief Extension a boss script must have. */
inline constexpr std::string_view SCRIPT_EXTENSION = ".wren";

/**
 * @brief Prefabs the game can spawn for each event type, and the parameters each one reads.
 *
 * Called with an event type (`spawn`, `boss`) and a prefab name. Returns the prefab parameters the loader may pass on
 * beyond `x` and `y` (e.g. `speed`), or nothing if this event type cannot use this prefab: the event is then skipped
 * with a warning, at load time rather than silently during the game.
 */
using PrefabCatalog = std::function<std::optional<std::vector<std::string>>(std::string_view, std::string_view)>;

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
 * type = "boss"              # stops the scroll until the boss is destroyed
 * prefab = "boss_core"
 * script = "scripts/Boss.wren"  # optional: relative path to a .wren file, without ".."
 * x = 1328.0                 # optional
 * y = 360.0                  # optional
 *
 * [[events]]
 * at = 6000.0
 * type = "end"
 *
 * [[terrain]]
 * from = 1800.0              # scroll distance where the section enters the screen, px
 * to = 3200.0                # scroll distance where it ends, px; must be greater than `from`
 * side = "bottom"            # "top" or "bottom"
 * height = 120.0             # optional, px, at most MAX_TERRAIN_HEIGHT
 * @endcode
 *
 * Never fails: every invalid value is corrected or its event skipped, and reported in `warnings`. A wave is expanded
 * into one spawn event per enemy, `interval` seconds apart at the scroll speed in effect where the wave starts.
 *
 * @param file Parsed level file.
 * @param prefabs Prefabs the game can spawn per event type; an event naming another one is skipped.
 * @param warnings Receives one message per corrected value or skipped event.
 * @return The level.
 */
[[nodiscard]] Level buildLevel(const config::ConfigFile& file, const PrefabCatalog& prefabs,
                               std::vector<std::string>& warnings);

/**
 * @brief Reads and builds a level file.
 * @param path Level file.
 * @param prefabs Prefabs the game can spawn per event type.
 * @param warnings Receives one message per corrected value or skipped event.
 * @return The level, or why the file could not be read or parsed.
 */
[[nodiscard]] std::expected<Level, std::string> loadLevel(const std::filesystem::path& path,
                                                          const PrefabCatalog& prefabs,
                                                          std::vector<std::string>& warnings);

}  // namespace rtype::server::level
