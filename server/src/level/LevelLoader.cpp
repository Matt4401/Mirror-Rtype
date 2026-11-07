/**
 * @file LevelLoader.cpp
 * @brief LevelLoader implementation.
 */

#include "level/LevelLoader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config/ConfigFile.hpp"
#include "config/ConfigReader.hpp"
#include "level/Level.hpp"
#include "prefabs/PrefabParams.hpp"
#include "timeline/TimelineEvent.hpp"

namespace rtype::server::level {

namespace {

/** @brief Keys of a spawn event read by the loader itself; every other key is passed to the prefab. */
constexpr std::array<std::string_view, 8> SPAWN_KEYS{"at",       "type", "prefab", "count",
                                                     "interval", "x",    "y",      "spacing_y"};

/** @brief An event of the file, before it is turned into timeline events. */
struct RawEvent {
    /** @brief Index in the `events` array, for messages and key lookups. */
    std::size_t index = 0;
    /** @brief Scroll distance. */
    float at = 0.0F;
    /** @brief Event type. */
    std::string type;
};

/**
 * @brief Builds the dotted key of a field of an event.
 * @param index Event index.
 * @param field Field name.
 * @return `events.<index>.<field>`.
 */
std::string eventKey(std::size_t index, std::string_view field) { return std::format("events.{}.{}", index, field); }

/**
 * @brief Copies a scalar value of the file into prefab parameters, whatever its type.
 * @param file Level file.
 * @param key Dotted key of the value.
 * @param name Parameter name.
 * @param params Receives the value.
 */
void copyValue(const config::ConfigFile& file, std::string_view key, std::string_view name,
               prefabs::PrefabParams& params) {
    if (const auto value = file.getBool(key)) {
        params.set(name, *value);
    } else if (const auto value = file.getInteger(key)) {
        params.set(name, *value);
    } else if (const auto value = file.getNumber(key)) {
        params.set(name, *value);
    } else if (const auto value = file.getString(key)) {
        params.set(name, *value);
    }
}

/**
 * @brief Reads the keys of a spawn event that the loader does not use, to pass them to the prefab.
 * @param file Level file.
 * @param index Event index.
 * @param warnings Receives a message per nested value, which cannot be passed on.
 * @return The extra parameters.
 */
prefabs::PrefabParams extraParams(const config::ConfigFile& file, std::size_t index,
                                  std::vector<std::string>& warnings) {
    prefabs::PrefabParams params;
    const std::string prefix = std::format("events.{}.", index);
    for (const std::string& key : file.keysUnder(std::format("events.{}", index))) {
        const std::string_view name = std::string_view(key).substr(prefix.size());
        if (name.contains('.')) {
            warnings.push_back(std::format("{}: nested values are not passed to the prefab, ignored", key));
            continue;
        }
        if (std::ranges::find(SPAWN_KEYS, name) == SPAWN_KEYS.end()) {
            copyValue(file, key, name, params);
        }
    }
    return params;
}

/**
 * @brief Reads the type and position of every event, skipping the unusable ones.
 * @param file Level file.
 * @param reader Reader reporting corrections.
 * @param warnings Receives a message per skipped event.
 * @return The events, sorted by position (ties keep the file order).
 */
std::vector<RawEvent> readEvents(const config::ConfigFile& file, config::ConfigReader& reader,
                                 std::vector<std::string>& warnings) {
    std::size_t count = file.arraySize("events");
    if (count > MAX_EVENTS) {
        warnings.push_back(std::format("events: {} events declared, only the first {} are kept", count, MAX_EVENTS));
        count = MAX_EVENTS;
    }
    std::vector<RawEvent> events;
    events.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string atKey = eventKey(i, "at");
        const std::optional<std::string> type = file.getString(eventKey(i, "type"));
        if (!type.has_value()) {
            warnings.push_back(std::format("events.{}: missing or invalid 'type', event skipped", i));
            continue;
        }
        if (!file.getNumber(atKey).has_value()) {
            warnings.push_back(std::format("events.{}: missing or invalid 'at', event skipped", i));
            continue;
        }
        const float at = reader.number(atKey, 0.0F, 0.0F, MAX_LEVEL_DISTANCE);
        events.push_back(RawEvent{.index = i, .at = at, .type = *type});
    }
    std::ranges::stable_sort(events, {}, &RawEvent::at);
    return events;
}

/**
 * @brief Adds the spawn events of a wave.
 * @param file Level file.
 * @param reader Reader reporting corrections.
 * @param event The wave.
 * @param scrollSpeed Scroll speed where the wave starts, used to space its enemies.
 * @param isKnownPrefab Prefabs the game can spawn.
 * @param out Level receiving the events.
 * @param warnings Receives a message if the wave is skipped.
 */
void addWave(const config::ConfigFile& file, config::ConfigReader& reader, const RawEvent& event, float scrollSpeed,
             const PrefabFilter& isKnownPrefab, Level& out, std::vector<std::string>& warnings) {
    const std::size_t i = event.index;
    const std::optional<std::string> prefab = file.getString(eventKey(i, "prefab"));
    if (!prefab.has_value()) {
        warnings.push_back(std::format("events.{}: spawn without 'prefab', event skipped", i));
        return;
    }
    if (!isKnownPrefab || !isKnownPrefab(*prefab)) {
        warnings.push_back(std::format("events.{}: unknown prefab '{}', event skipped", i, *prefab));
        return;
    }
    const std::size_t count = reader.count(eventKey(i, "count"), 1, 1, MAX_WAVE_COUNT);
    const float interval = reader.number(eventKey(i, "interval"), 0.0F, 0.0F, MAX_WAVE_INTERVAL);
    const float x = reader.number(eventKey(i, "x"), DEFAULT_SPAWN_X, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE);
    const float y = reader.number(eventKey(i, "y"), DEFAULT_SPAWN_Y, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE);
    const float spacingY = reader.number(eventKey(i, "spacing_y"), 0.0F, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE);
    const prefabs::PrefabParams extra = extraParams(file, i, warnings);

    const float step = interval * scrollSpeed;
    for (std::size_t n = 0; n < count; ++n) {
        const auto rank = static_cast<float>(n);
        prefabs::PrefabParams params = extra;
        params.set("prefab", *prefab);
        params.set("x", static_cast<double>(x));
        params.set("y",
                   static_cast<double>(std::clamp(y + (rank * spacingY), -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE)));
        out.timeline.add(timeline::TimelineEvent{
            .position = std::min(event.at + (rank * step), MAX_LEVEL_DISTANCE),
            .type = std::string(EVENT_SPAWN),
            .params = std::move(params),
        });
    }
}

}  // namespace

Level buildLevel(const config::ConfigFile& file, const PrefabFilter& isKnownPrefab,
                 std::vector<std::string>& warnings) {
    config::ConfigReader reader(file, warnings);
    Level level;
    level.name = reader.text("level.name", "Unnamed level");
    level.scrollSpeed = reader.number("level.scroll_speed", DEFAULT_SCROLL_SPEED, 0.0F, MAX_SCROLL_SPEED);
    reader.reportUnknownKeys("level");

    float scrollSpeed = level.scrollSpeed;
    for (const RawEvent& event : readEvents(file, reader, warnings)) {
        if (event.type == EVENT_SPAWN) {
            addWave(file, reader, event, scrollSpeed, isKnownPrefab, level, warnings);
        } else if (event.type == EVENT_SCROLL) {
            if (!file.getNumber(eventKey(event.index, "speed")).has_value()) {
                warnings.push_back(std::format("events.{}: scroll without 'speed', event skipped", event.index));
                continue;
            }
            scrollSpeed = reader.number(eventKey(event.index, "speed"), scrollSpeed, 0.0F, MAX_SCROLL_SPEED);
            level.timeline.add(timeline::TimelineEvent{
                .position = event.at,
                .type = std::string(EVENT_SCROLL),
                .params = {{"speed", static_cast<double>(scrollSpeed)}},
            });
        } else if (event.type == EVENT_END) {
            level.timeline.add(
                timeline::TimelineEvent{.position = event.at, .type = std::string(EVENT_END), .params = {}});
        } else {
            warnings.push_back(std::format("events.{}: unknown type '{}', event skipped", event.index, event.type));
        }
    }
    return level;
}

std::expected<Level, std::string> loadLevel(const std::filesystem::path& path, const PrefabFilter& isKnownPrefab,
                                            std::vector<std::string>& warnings) {
    const config::ConfigFile file = config::ConfigFile::load(path);
    if (file.error().has_value()) {
        return std::unexpected(*file.error());
    }
    warnings.insert(warnings.end(), file.warnings().begin(), file.warnings().end());
    return buildLevel(file, isKnownPrefab, warnings);
}

}  // namespace rtype::server::level
