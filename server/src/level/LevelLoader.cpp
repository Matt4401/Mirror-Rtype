/**
 * @file LevelLoader.cpp
 * @brief LevelLoader implementation.
 */

#include "level/LevelLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config/ConfigFile.hpp"
#include "config/ConfigReader.hpp"
#include "game/ReplicationCodec.hpp"
#include "level/Level.hpp"
#include "prefabs/PrefabParams.hpp"
#include "timeline/TimelineEvent.hpp"

namespace rtype::server::level {

namespace {

/** @brief Keys of a spawn event read by the loader itself; every other key is passed to the prefab. */
constexpr std::array<std::string_view, 8> SPAWN_KEYS{"at",       "type", "prefab", "count",
                                                     "interval", "x",    "y",      "spacing_y"};

/** @brief Keys of a boss event read by the loader itself; every other key is passed to the prefab. */
constexpr std::array<std::string_view, 8> BOSS_KEYS{"at", "type", "prefab", "script", "class", "x", "y", "parts"};

/** @brief Keys of a boss part (`[[events.parts]]`). */
constexpr std::array<std::string_view, 7> PART_KEYS{"role", "x", "y", "width", "height", "health", "look"};

/** @brief Keys of a scroll event. */
constexpr std::array<std::string_view, 3> SCROLL_KEYS{"at", "type", "speed"};

/** @brief Keys of an end event. */
constexpr std::array<std::string_view, 2> END_KEYS{"at", "type"};

/** @brief The prefab of a spawn or boss event, and the parameters it reads. */
struct EventPrefab {
    /** @brief Prefab name. */
    std::string name;
    /** @brief Parameters the prefab reads beyond `x` and `y`. */
    std::vector<std::string> params;
};

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
 * @brief Reads the keys of an event that the loader does not use, to pass them to the prefab.
 *
 * Only the parameters the prefab reads are passed on: any other key (usually a typo, such as `cout = 5`) is reported
 * and ignored.
 *
 * @param file Level file.
 * @param index Event index.
 * @param readKeys Keys the loader reads itself, not passed on.
 * @param prefab Prefab of the event and the parameters it reads.
 * @param warnings Receives a message per ignored key.
 * @return The extra parameters.
 */
prefabs::PrefabParams extraParams(const config::ConfigFile& file, std::size_t index,
                                  std::span<const std::string_view> readKeys, const EventPrefab& prefab,
                                  std::vector<std::string>& warnings) {
    prefabs::PrefabParams params;
    const std::string prefix = std::format("events.{}.", index);
    for (const std::string& key : file.keysUnder(std::format("events.{}", index))) {
        const std::string_view name = std::string_view(key).substr(prefix.size());
        // Nested values the loader reads itself (the boss parts) are not reported.
        if (std::ranges::find(readKeys, name.substr(0, name.find('.'))) != readKeys.end()) {
            continue;
        }
        if (name.contains('.')) {
            warnings.push_back(std::format("{}: nested values are not passed to the prefab, ignored", key));
            continue;
        }
        if (std::ranges::find(readKeys, name) != readKeys.end()) {
            continue;
        }
        if (std::ranges::find(prefab.params, name) == prefab.params.end()) {
            warnings.push_back(std::format("{}: not a parameter of '{}', ignored", key, prefab.name));
            continue;
        }
        copyValue(file, key, name, params);
    }
    return params;
}

/**
 * @brief Reports the keys of an event that its type does not read, usually typos.
 * @param file Level file.
 * @param index Event index.
 * @param keys Keys the event type reads.
 * @param warnings Receives a message per unknown key.
 */
void reportUnknownEventKeys(const config::ConfigFile& file, std::size_t index, std::span<const std::string_view> keys,
                            std::vector<std::string>& warnings) {
    const std::string prefix = std::format("events.{}.", index);
    for (const std::string& key : file.keysUnder(std::format("events.{}", index))) {
        if (std::ranges::find(keys, std::string_view(key).substr(prefix.size())) == keys.end()) {
            warnings.push_back(std::format("{}: unknown key, ignored", key));
        }
    }
}

/**
 * @brief Tells whether a key holds a finite number.
 * @param file Level file.
 * @param key Dotted key.
 * @return False if the key is missing, is not a number, or is `nan` / `inf`.
 */
bool isFiniteNumber(const config::ConfigFile& file, std::string_view key) {
    const std::optional<double> value = file.getNumber(key);
    return value.has_value() && std::isfinite(*value);
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
        // A non-finite `at` (nan, inf, or a literal that overflows) would be replaced by the reader's fallback (0) and
        // fire on the first step: the event is skipped instead.
        if (!isFiniteNumber(file, atKey)) {
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
 * @brief Reads the prefab of a spawn or boss event, checking that the game can spawn it for this event type.
 * @param file Level file.
 * @param event The event.
 * @param prefabs Prefabs the game can spawn per event type.
 * @param warnings Receives a message if the prefab is missing, unknown, or not for this event type.
 * @return The prefab and the parameters it reads, or nothing if the event must be skipped.
 */
std::optional<EventPrefab> readPrefab(const config::ConfigFile& file, const RawEvent& event,
                                      const PrefabCatalog& prefabs, std::vector<std::string>& warnings) {
    const std::optional<std::string> prefab = file.getString(eventKey(event.index, "prefab"));
    if (!prefab.has_value()) {
        warnings.push_back(std::format("events.{}: {} without 'prefab', event skipped", event.index, event.type));
        return std::nullopt;
    }
    std::optional<std::vector<std::string>> params =
        prefabs ? prefabs(event.type, *prefab) : std::optional<std::vector<std::string>>{};
    if (!params.has_value()) {
        warnings.push_back(std::format("events.{}: prefab '{}' cannot be used by a {} event, event skipped",
                                       event.index, *prefab, event.type));
        return std::nullopt;
    }
    return EventPrefab{.name = *prefab, .params = std::move(*params)};
}

/**
 * @brief Tells whether a boss script path is acceptable: a relative `.wren` file, with `/` separators and no `..`,
 * so it stays under the directory the server runs from.
 * @param path Path read from the file.
 * @return True if the path can be handed to the scripting plugin.
 */
bool isValidScriptPath(std::string_view path) {
    // Backslashes are refused so that a file accepted on Linux means the same path on Windows.
    if (path.size() <= SCRIPT_EXTENSION.size() || path.size() > MAX_SCRIPT_PATH_LENGTH ||
        !path.ends_with(SCRIPT_EXTENSION) || path.contains('\\')) {
        return false;
    }
    const std::filesystem::path script(path);
    if (script.is_absolute() || script.has_root_name() || script.has_root_directory()) {
        return false;
    }
    return std::ranges::none_of(script, [](const std::filesystem::path& part) { return part == ".."; });
}

/**
 * @brief Reads the parts of a boss event into its parameters: `parts.count`, then `parts.<n>.<key>` per part.
 *
 * The role is passed as is: the game skips a part whose role it does not know. Offsets are clamped to
 * `MAX_BOSS_PART_OFFSET`, the other values are passed to the part prefab, which clamps them.
 *
 * @param file Level file.
 * @param reader Reader reporting corrections.
 * @param index Event index.
 * @param params Receives the parts.
 * @param warnings Receives a message per part skipped and per unknown key.
 */
void readBossParts(const config::ConfigFile& file, config::ConfigReader& reader, const std::size_t index,
                   prefabs::PrefabParams& params, std::vector<std::string>& warnings) {
    const std::string array = eventKey(index, "parts");
    std::size_t count = file.arraySize(array);
    if (count > MAX_BOSS_PARTS) {
        warnings.push_back(
            std::format("{}: {} parts declared, only the first {} are kept", array, count, MAX_BOSS_PARTS));
        count = MAX_BOSS_PARTS;
    }
    std::size_t kept = 0;
    for (std::size_t part = 0; part < count; ++part) {
        const std::string prefix = std::format("{}.{}", array, part);
        const auto key = [&prefix](const std::string_view field) { return std::format("{}.{}", prefix, field); };
        const std::optional<std::string> role = file.getString(key("role"));
        if (!role.has_value()) {
            warnings.push_back(std::format("{}: missing or invalid 'role', part skipped", prefix));
            continue;
        }
        for (const std::string& name : file.keysUnder(prefix)) {
            if (std::ranges::find(PART_KEYS, std::string_view(name).substr(prefix.size() + 1)) == PART_KEYS.end()) {
                warnings.push_back(std::format("{}: unknown key, ignored", name));
            }
        }
        const std::string out = std::format("parts.{}.", kept);
        params.set(out + "role", *role);
        params.set(out + "x",
                   static_cast<double>(reader.number(key("x"), 0.0F, -MAX_BOSS_PART_OFFSET, MAX_BOSS_PART_OFFSET)));
        params.set(out + "y",
                   static_cast<double>(reader.number(key("y"), 0.0F, -MAX_BOSS_PART_OFFSET, MAX_BOSS_PART_OFFSET)));
        for (const std::string_view field : {"width", "height", "health", "look"}) {
            const std::optional<double> value = file.getNumber(key(field));
            if (value.has_value() && std::isfinite(*value)) {
                params.set(out + std::string(field), *value);
            }
        }
        ++kept;
    }
    params.set("parts.count", static_cast<std::int64_t>(kept));
}

/**
 * @brief Adds a boss event.
 * @param file Level file.
 * @param reader Reader reporting corrections.
 * @param event The boss event.
 * @param prefabs Prefabs the game can spawn per event type.
 * @param out Receives the event.
 * @param warnings Receives a message if the event is skipped or its script ignored.
 */
void addBoss(const config::ConfigFile& file, config::ConfigReader& reader, const RawEvent& event,
             const PrefabCatalog& prefabs, std::vector<timeline::TimelineEvent>& out,
             std::vector<std::string>& warnings) {
    const std::size_t i = event.index;
    const std::optional<EventPrefab> prefab = readPrefab(file, event, prefabs, warnings);
    if (!prefab.has_value()) {
        return;
    }
    prefabs::PrefabParams params = extraParams(file, i, BOSS_KEYS, *prefab, warnings);
    params.set("prefab", prefab->name);
    if (const auto className = file.getString(eventKey(i, "class"))) {
        params.set("class", *className);
    }
    params.set("x", static_cast<double>(
                        reader.number(eventKey(i, "x"), DEFAULT_SPAWN_X, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE)));
    params.set("y", static_cast<double>(
                        reader.number(eventKey(i, "y"), DEFAULT_SPAWN_Y, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE)));
    if (file.contains(eventKey(i, "script"))) {
        const std::optional<std::string> script = file.getString(eventKey(i, "script"));
        if (script.has_value() && isValidScriptPath(*script)) {
            params.set("script", *script);
        } else {
            warnings.push_back(std::format(
                "events.{}: 'script' must be a relative path to a {} file without '..', ignored", i, SCRIPT_EXTENSION));
        }
    }
    readBossParts(file, reader, i, params, warnings);
    out.push_back(timeline::TimelineEvent{
        .position = event.at,
        .type = std::string(EVENT_BOSS),
        .params = std::move(params),
    });
}

/** @brief Scroll speed set from a distance onwards, by the level or a `scroll` event. */
struct SpeedChange {
    /** @brief Scroll distance where the speed changes. */
    float at = 0.0F;
    /** @brief New speed, px/s, at least MIN_SCROLL_SPEED. */
    float speed = 0.0F;
};

/**
 * @brief Finds where the scroll is after scrolling for some time, across the speed changes on the way.
 *
 * The time only counts while the scroll moves, so a pause (boss fight, respawn) delays the result too.
 *
 * @param speeds Speed changes sorted by distance, the first one at 0.
 * @param from Scroll distance at the start.
 * @param seconds Scroll time.
 * @return Scroll distance reached.
 */
float scrollAfter(const std::vector<SpeedChange>& speeds, float from, float seconds) {
    auto next = std::ranges::upper_bound(speeds, from, {}, &SpeedChange::at);
    float speed = std::prev(next)->speed;
    float position = from;
    for (; next != speeds.end(); ++next) {
        const float untilChange = (next->at - position) / speed;
        if (untilChange >= seconds) {
            break;
        }
        seconds -= untilChange;
        position = next->at;
        speed = next->speed;
    }
    return position + (seconds * speed);
}

/**
 * @brief Adds the spawn events of a wave.
 * @param file Level file.
 * @param reader Reader reporting corrections.
 * @param event The wave.
 * @param speeds Scroll speeds of the level, used to space its enemies in time.
 * @param prefabs Prefabs the game can spawn per event type.
 * @param out Receives the events.
 * @param warnings Receives a message if the wave is skipped.
 */
void addWave(const config::ConfigFile& file, config::ConfigReader& reader, const RawEvent& event,
             const std::vector<SpeedChange>& speeds, const PrefabCatalog& prefabs,
             std::vector<timeline::TimelineEvent>& out, std::vector<std::string>& warnings) {
    const std::size_t i = event.index;
    const std::optional<EventPrefab> prefab = readPrefab(file, event, prefabs, warnings);
    if (!prefab.has_value()) {
        return;
    }
    const std::size_t count = reader.count(eventKey(i, "count"), 1, 1, MAX_WAVE_COUNT);
    const float interval = reader.number(eventKey(i, "interval"), 0.0F, 0.0F, MAX_WAVE_INTERVAL);
    const float x = reader.number(eventKey(i, "x"), DEFAULT_SPAWN_X, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE);
    const float y = reader.number(eventKey(i, "y"), DEFAULT_SPAWN_Y, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE);
    const float spacingY = reader.number(eventKey(i, "spacing_y"), 0.0F, -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE);
    const prefabs::PrefabParams extra = extraParams(file, i, SPAWN_KEYS, *prefab, warnings);

    float position = event.at;
    for (std::size_t n = 0; n < count; ++n) {
        const auto rank = static_cast<float>(n);
        prefabs::PrefabParams params = extra;
        params.set("prefab", prefab->name);
        params.set("x", static_cast<double>(x));
        params.set("y",
                   static_cast<double>(std::clamp(y + (rank * spacingY), -MAX_SPAWN_COORDINATE, MAX_SPAWN_COORDINATE)));
        out.push_back(timeline::TimelineEvent{
            .position = position,
            .type = std::string(EVENT_SPAWN),
            .params = std::move(params),
        });
        position = std::min(scrollAfter(speeds, position, interval), MAX_LEVEL_DISTANCE);
    }
}

/** @brief A terrain section accepted so far, to check the next ones against it. */
struct TerrainSpan {
    /** @brief Scroll distance where it starts. */
    float from = 0.0F;
    /** @brief Scroll distance where it ends. */
    float to = 0.0F;
    /** @brief True along the top edge. */
    bool top = false;
    /** @brief Height in pixels. */
    float height = 0.0F;
};

/**
 * @brief Checks a section against the ones accepted before it.
 *
 * Two sections on the same side cannot overlap: their columns would stack. A top and a bottom section that overlap must
 * leave `MIN_TERRAIN_CORRIDOR` between them, so the new one is lowered if needed.
 *
 * @param section Section to check; its height may be lowered.
 * @param accepted Sections accepted so far.
 * @param index Index of the section, for messages.
 * @param warnings Receives a message if the section is lowered or skipped.
 * @return False if the section must be skipped.
 */
bool fitTerrain(TerrainSpan& section, std::span<const TerrainSpan> accepted, std::size_t index,
                std::vector<std::string>& warnings) {
    for (const TerrainSpan& other : accepted) {
        if (section.from >= other.to || other.from >= section.to) {
            continue;
        }
        if (other.top == section.top) {
            warnings.push_back(
                std::format("terrain.{}: overlaps another section on the same side, section skipped", index));
            return false;
        }
        const float room = TERRAIN_SCENE_HEIGHT - MIN_TERRAIN_CORRIDOR - other.height;
        if (room < 1.0F) {
            warnings.push_back(std::format("terrain.{}: no room left by the opposite section, section skipped", index));
            return false;
        }
        if (section.height > room) {
            warnings.push_back(std::format("terrain.{}: lowered to {} px to keep a {} px corridor", index, room,
                                           MIN_TERRAIN_CORRIDOR));
            section.height = room;
        }
    }
    return true;
}

/**
 * @brief Adds the `terrain` events of the `[[terrain]]` sections.
 * @param file Level file.
 * @param reader Reader reporting corrections.
 * @param out Receives the events.
 * @param warnings Receives a message per skipped section.
 */
void addTerrain(const config::ConfigFile& file, config::ConfigReader& reader, std::vector<timeline::TimelineEvent>& out,
                std::vector<std::string>& warnings) {
    std::size_t count = file.arraySize("terrain");
    if (count > MAX_TERRAIN_SECTIONS) {
        warnings.push_back(
            std::format("terrain: {} sections declared, only the first {} are kept", count, MAX_TERRAIN_SECTIONS));
        count = MAX_TERRAIN_SECTIONS;
    }
    std::vector<TerrainSpan> accepted;
    for (std::size_t i = 0; i < count; ++i) {
        const std::string fromKey = std::format("terrain.{}.from", i);
        const std::string toKey = std::format("terrain.{}.to", i);
        if (!isFiniteNumber(file, fromKey) || !isFiniteNumber(file, toKey)) {
            warnings.push_back(std::format("terrain.{}: missing or invalid 'from' or 'to', section skipped", i));
            continue;
        }
        const float from = reader.number(fromKey, 0.0F, 0.0F, MAX_LEVEL_DISTANCE);
        const float to = reader.number(toKey, 0.0F, 0.0F, MAX_LEVEL_DISTANCE);
        if (to <= from) {
            warnings.push_back(std::format("terrain.{}: 'to' must be greater than 'from', section skipped", i));
            continue;
        }
        const std::string side = reader.text(std::format("terrain.{}.side", i), "");
        if (side != TERRAIN_TOP && side != TERRAIN_BOTTOM) {
            warnings.push_back(std::format(R"(terrain.{}: 'side' must be "{}" or "{}", section skipped)", i,
                                           TERRAIN_TOP, TERRAIN_BOTTOM));
            continue;
        }
        // Built in whole blocks: the section really ends there, which the overlap checks must see.
        const float blocks = std::ceil((to - from) / game::TERRAIN_BLOCK_WIDTH);
        TerrainSpan section{
            .from = from,
            .to = std::min(from + (blocks * game::TERRAIN_BLOCK_WIDTH), MAX_LEVEL_DISTANCE),
            .top = side == TERRAIN_TOP,
            .height =
                reader.number(std::format("terrain.{}.height", i), DEFAULT_TERRAIN_HEIGHT, 1.0F, MAX_TERRAIN_HEIGHT),
        };
        reader.reportUnknownKeys(std::format("terrain.{}", i));
        if (!fitTerrain(section, accepted, i, warnings)) {
            continue;
        }
        accepted.push_back(section);
        out.push_back(timeline::TimelineEvent{
            .position = from,
            .type = std::string(EVENT_TERRAIN),
            .params = {{"side", side},
                       {"height", static_cast<double>(section.height)},
                       {"length", static_cast<double>(section.to - from)}},
        });
    }
}

}  // namespace

Level buildLevel(const config::ConfigFile& file, const PrefabCatalog& prefabs, std::vector<std::string>& warnings) {
    config::ConfigReader reader(file, warnings);
    Level level;
    std::vector<timeline::TimelineEvent> events;
    level.name = reader.text("level.name", "Unnamed level");
    level.scrollSpeed = reader.number("level.scroll_speed", DEFAULT_SCROLL_SPEED, MIN_SCROLL_SPEED, MAX_SCROLL_SPEED);
    reader.reportUnknownKeys("level");

    float scrollSpeed = level.scrollSpeed;
    // Waves are expanded once every speed change is known, since a wave may span a `scroll` event.
    std::vector<SpeedChange> speeds{{.at = 0.0F, .speed = level.scrollSpeed}};
    std::vector<RawEvent> waves;
    for (const RawEvent& event : readEvents(file, reader, warnings)) {
        if (event.type == EVENT_SPAWN) {
            waves.push_back(event);
        } else if (event.type == EVENT_SCROLL) {
            reportUnknownEventKeys(file, event.index, SCROLL_KEYS, warnings);
            if (!file.getNumber(eventKey(event.index, "speed")).has_value()) {
                warnings.push_back(std::format("events.{}: scroll without 'speed', event skipped", event.index));
                continue;
            }
            scrollSpeed =
                reader.number(eventKey(event.index, "speed"), scrollSpeed, MIN_SCROLL_SPEED, MAX_SCROLL_SPEED);
            speeds.push_back({.at = event.at, .speed = scrollSpeed});
            events.push_back(timeline::TimelineEvent{
                .position = event.at,
                .type = std::string(EVENT_SCROLL),
                .params = {{"speed", static_cast<double>(scrollSpeed)}},
            });
        } else if (event.type == EVENT_BOSS) {
            addBoss(file, reader, event, prefabs, events, warnings);
        } else if (event.type == EVENT_END) {
            reportUnknownEventKeys(file, event.index, END_KEYS, warnings);
            events.push_back(
                timeline::TimelineEvent{.position = event.at, .type = std::string(EVENT_END), .params = {}});
        } else {
            warnings.push_back(std::format("events.{}: unknown type '{}', event skipped", event.index, event.type));
        }
    }
    for (const RawEvent& wave : waves) {
        addWave(file, reader, wave, speeds, prefabs, events, warnings);
    }
    addTerrain(file, reader, events, warnings);
    // One sort for the whole level: adding the events one by one would be quadratic.
    level.timeline.addAll(std::move(events));
    return level;
}

std::expected<Level, std::string> loadLevel(const std::filesystem::path& path, const PrefabCatalog& prefabs,
                                            std::vector<std::string>& warnings) {
    const config::ConfigFile file = config::ConfigFile::load(path);
    if (file.error().has_value()) {
        return std::unexpected(*file.error());
    }
    warnings.insert(warnings.end(), file.warnings().begin(), file.warnings().end());
    return buildLevel(file, prefabs, warnings);
}

}  // namespace rtype::server::level
