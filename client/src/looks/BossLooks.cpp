#include "looks/BossLooks.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"
#include "config/ConfigReader.hpp"

namespace client::looks {

namespace {

/** @brief Largest id of a look: they travel in a byte. */
constexpr std::size_t MAX_LOOK_ID = 255;
/** @brief Shortest frame duration, in seconds. */
constexpr float MIN_FRAME_DURATION = 0.01F;
/** @brief Longest frame duration, in seconds. */
constexpr float MAX_FRAME_DURATION = 10.0F;

/**
 * @brief Tells whether a texture path stays under the directory the client runs from.
 * @param path Path read from the file.
 * @return False for an empty, absolute or `..` path, or one with backslashes.
 */
bool isSafeTexturePath(const std::string_view path) {
    if (path.empty() || path.contains('\\')) {
        return false;
    }
    const std::filesystem::path texture(path);
    if (texture.is_absolute() || texture.has_root_name() || texture.has_root_directory()) {
        return false;
    }
    return std::ranges::none_of(texture, [](const std::filesystem::path& part) { return part == ".."; });
}

/**
 * @brief Reads the sprite of an entry, if it names a texture.
 * @param file Looks file.
 * @param reader Reader reporting corrections.
 * @param entry Dotted key of the entry, such as `boss.0`.
 * @return The sprite, or nothing without a usable texture and frame size.
 */
std::optional<SpriteLook> readSprite(const config::ConfigFile& file, config::ConfigReader& reader,
                                     const std::string& entry) {
    const auto key = [&entry](const std::string_view field) { return std::format("{}.{}", entry, field); };
    // Every key is read first, so that none of them is reported as unknown when the sprite is ignored.
    SpriteLook sprite{.texture = reader.text(key("texture"), "")};
    sprite.x = static_cast<int>(reader.count(key("frame_x"), 0, 0, MAX_FRAME_PIXELS));
    sprite.y = static_cast<int>(reader.count(key("frame_y"), 0, 0, MAX_FRAME_PIXELS));
    sprite.frameWidth = static_cast<int>(reader.count(key("frame_width"), 0, 0, MAX_FRAME_PIXELS));
    sprite.frameHeight = static_cast<int>(reader.count(key("frame_height"), 0, 0, MAX_FRAME_PIXELS));
    sprite.frames = static_cast<int>(reader.count(key("frames"), 1, 1, MAX_FRAMES));
    sprite.frameDuration =
        reader.number(key("frame_duration"), sprite.frameDuration, MIN_FRAME_DURATION, MAX_FRAME_DURATION);
    sprite.loop = reader.flag(key("loop"), true);
    sprite.scale = reader.number(key("scale"), 1.0F, MIN_SCALE, MAX_SCALE);
    if (!file.contains(key("texture"))) {
        return std::nullopt;
    }
    if (!isSafeTexturePath(sprite.texture)) {
        reader.warn(std::format("{}: texture must be a relative path without '..', sprite ignored", key("texture")));
        return std::nullopt;
    }
    if (sprite.frameWidth == 0 || sprite.frameHeight == 0) {
        reader.warn(std::format("{}: a sprite needs frame_width and frame_height, sprite ignored", entry));
        return std::nullopt;
    }
    return sprite;
}

/**
 * @brief Reads the entries of one array (`boss` or `part`).
 * @param file Looks file.
 * @param reader Reader reporting corrections.
 * @param array Array name.
 * @param idKey Key holding the id of an entry (`variant` or `look`).
 * @param out Receives the looks by id.
 */
void readEntries(const config::ConfigFile& file, config::ConfigReader& reader, const std::string_view array,
                 const std::string_view idKey, std::map<std::uint8_t, Look>& out) {
    std::size_t count = file.arraySize(array);
    if (count > MAX_LOOKS) {
        reader.warn(std::format("{}: {} entries, only the first {} are kept", array, count, MAX_LOOKS));
        count = MAX_LOOKS;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const std::string entry = std::format("{}.{}", array, i);
        const std::string id = std::format("{}.{}", entry, idKey);
        if (!file.getInteger(id).has_value()) {
            reader.warn(std::format("{}: missing or invalid '{}', entry ignored", entry, idKey));
            continue;
        }
        const auto key = static_cast<std::uint8_t>(reader.count(id, 0, 0, MAX_LOOK_ID));
        Look look;
        look.width = reader.number(std::format("{}.width", entry), 0.0F, 0.0F, MAX_BOX_SIZE);
        look.height = reader.number(std::format("{}.height", entry), 0.0F, 0.0F, MAX_BOX_SIZE);
        look.sprite = readSprite(file, reader, entry);
        reader.reportUnknownKeys(entry);
        out[key] = look;
    }
}

}  // namespace

BossLooks readBossLooks(const config::ConfigFile& file, std::vector<std::string>& warnings) {
    config::ConfigReader reader(file, warnings);
    BossLooks looks;
    readEntries(file, reader, "boss", "variant", looks.bosses);
    readEntries(file, reader, "part", "look", looks.parts);
    return looks;
}

}  // namespace client::looks
