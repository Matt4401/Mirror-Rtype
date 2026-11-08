/**
 * @file Level.hpp
 * @brief R-Type level: its settings and the timeline of events driven by the scroll distance.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "timeline/Timeline.hpp"

namespace rtype::server::level {

/** @brief Event type spawning one entity: `prefab` plus its parameters (`x`, `y`, and any extra key). */
inline constexpr std::string_view EVENT_SPAWN = "spawn";

/** @brief Event type changing the scroll speed: `speed` in pixels per second, at least `MIN_SCROLL_SPEED`. */
inline constexpr std::string_view EVENT_SCROLL = "scroll";

/**
 * @brief Event type starting a boss fight: `prefab`, `x`, `y`, an optional `script` (Wren file driving the boss) and
 * any extra key. The scroll stops until the boss is destroyed.
 */
inline constexpr std::string_view EVENT_BOSS = "boss";

/**
 * @brief Event type starting a terrain section, from `[[terrain]]`: `side` (`"top"` or `"bottom"`), `height` and
 * `length` in pixels. The section enters from the right edge and scrolls with the level; touching it kills a ship.
 */
inline constexpr std::string_view EVENT_TERRAIN = "terrain";

/** @brief `side` of a terrain section along the top edge of the screen. */
inline constexpr std::string_view TERRAIN_TOP = "top";

/** @brief `side` of a terrain section along the bottom edge of the screen. */
inline constexpr std::string_view TERRAIN_BOTTOM = "bottom";

/** @brief Event type ending the level. */
inline constexpr std::string_view EVENT_END = "end";

/**
 * @brief A loaded level.
 *
 * Timeline positions are scroll distances in pixels: the gameplay moves the cursor at the current scroll speed.
 * Waves are already expanded: a wave of 5 enemies is 5 `spawn` events. Terrain sections are `terrain` events placed
 * at their start distance.
 */
struct Level {
    /** @brief Display name. */
    std::string name;

    /** @brief Scroll speed at the start of the level, in pixels per second. */
    float scrollSpeed = 0.0F;

    /** @brief Events by scroll distance. */
    timeline::Timeline timeline;

    /**
     * @brief Directory the relative `script` paths of the boss events are resolved from; empty for the current
     * directory. Set by the server to its data directory, so that it does not depend on where it is started from.
     */
    std::filesystem::path dataRoot;

    /**
     * @brief Level played after this one in a campaign, or `nullptr` for the last level.
     *
     * The server chains the level files it finds (`server/levels/level_*.toml`, by name) through this pointer.
     */
    std::shared_ptr<const Level> next;
};

/**
 * @brief Counts the levels of a campaign, from its first level.
 * @param first First level, or `nullptr`.
 * @return Number of levels reached through `Level::next`, 0 for `nullptr`.
 */
[[nodiscard]] inline std::size_t campaignLength(const Level* first) noexcept {
    std::size_t count = 0;
    for (const Level* level = first; level != nullptr; level = level->next.get()) {
        ++count;
    }
    return count;
}

}  // namespace rtype::server::level
