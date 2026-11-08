/**
 * @file Looks.hpp
 * @brief How the client draws the bosses, their parts and the other enemies, read from `client/config/looks.toml`.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config/ConfigFile.hpp"

namespace client::looks {

/** @brief Path of the looks file, from the directory the client runs from. */
inline constexpr const char* LOOKS_PATH = "client/config/looks.toml";
/** @brief Most `[[boss]]`, `[[part]]` or `[[entity]]` entries read; the next ones are ignored, with a warning. */
inline constexpr std::size_t MAX_LOOKS = 64;
/** @brief Prefabs an `[[entity]]` entry may give a look to. */
inline constexpr std::array<std::string_view, 5> ENTITY_PREFABS{"alien_scout", "alien_gunner", "alien_elite", "missile",
                                                                "power_up"};
/** @brief Largest frame side and frame origin coordinate, in texture pixels. */
inline constexpr int MAX_FRAME_PIXELS = 4096;
/** @brief Most frames of an animation strip. */
inline constexpr int MAX_FRAMES = 64;
/** @brief Smallest and largest drawing scale of a sprite. */
inline constexpr float MIN_SCALE = 0.1F;
/** @brief Largest drawing scale of a sprite. */
inline constexpr float MAX_SCALE = 16.0F;
/** @brief Largest box side, in pixels. */
inline constexpr float MAX_BOX_SIZE = 2048.0F;

/** @brief An animation strip of a spritesheet: `frames` frames of the same size, side by side from (`x`, `y`). */
struct SpriteLook {
    /** @brief Texture path, from the directory the client runs from. */
    std::string texture;
    /** @brief Left of the first frame, in texture pixels. */
    int x = 0;
    /** @brief Top of the strip, in texture pixels. */
    int y = 0;
    /** @brief Width of a frame, in texture pixels. */
    int frameWidth = 0;
    /** @brief Height of a frame, in texture pixels. */
    int frameHeight = 0;
    /** @brief Number of frames. */
    int frames = 1;
    /** @brief Seconds per frame. */
    float frameDuration = 0.1F;
    /** @brief True to loop the animation, false to stop on the last frame. */
    bool loop = true;
    /** @brief Drawing scale. */
    float scale = 1.0F;
    /** @brief Pixels from one frame to the next; 0 for `frameWidth`, frames that touch. */
    int frameStride = 0;
    /** @brief True if the frames are directions around the full turn: the client shows the one facing the ships. */
    bool directional = false;
    /** @brief Direction of the first frame of a directional sprite, in degrees (0 right, 90 down). */
    float firstAngle = 0.0F;
    /** @brief True to draw the sprite mirrored left to right, for a sheet facing the other way. */
    bool flipX = false;
    /** @brief True to turn the sprite towards where it moves; the sheet faces right (left when mirrored). */
    bool faceMotion = false;
    /** @brief Widths of frames that differ, side by side from `x`; empty when every frame is `frameWidth` wide. */
    std::vector<int> frameWidths;
};

/** @brief Look of a boss variant or of a boss part look. */
struct Look {
    /** @brief Width of the box drawn without a sprite, in pixels; 0 keeps the default size. */
    float width = 0.0F;
    /** @brief Height of the box drawn without a sprite, in pixels; 0 keeps the default size. */
    float height = 0.0F;
    /** @brief Sprite, if the entry names a texture. */
    std::optional<SpriteLook> sprite;
};

/**
 * @brief Every configured look: bosses by `variant`, parts by `look`, other entities by prefab name. A missing entry
 * keeps the default look of the client.
 */
struct Looks {
    /** @brief Looks of the boss variants. */
    std::map<std::uint8_t, Look> bosses;
    /** @brief Looks of the boss parts. */
    std::map<std::uint8_t, Look> parts;
    /** @brief Looks of the other entities (`ENTITY_PREFABS`), by prefab name, whatever their `subtype`. */
    std::map<std::string, Look> entities;
    /** @brief Looks of the other entities with a given `subtype` (such as the missiles of one shooter). */
    std::map<std::pair<std::string, std::uint8_t>, Look> entitySubtypes;
    /** @brief Sprite of one explosion burst (`[explosion]`), played once; without it, bursts are growing boxes. */
    std::optional<SpriteLook> explosion;
};

/**
 * @brief Reads the looks of a configuration file.
 *
 * Reads the `[[boss]]` entries (key `variant`), the `[[part]]` entries (key `look`) and the `[[entity]]` entries (key
 * `prefab`, one of `ENTITY_PREFABS`, and optionally `subtype`, 0–255). Each entry sets `width` and
 * `height` (box drawn without a sprite) and, optionally, a sprite: `texture`, `frame_x`, `frame_y`, `frame_width`,
 * `frame_height`, `frame_stride`, `frame_widths`, `frames`, `frame_duration`, `loop`, `scale`, `directional`,
 * `first_angle`, `flip_x` and `face_motion`.
 * Out-of-range values are clamped; an entry without its id, a sprite without its frame size, an unsafe texture path
 * (absolute, with `..` or `\`) and unknown keys are reported in `warnings` and ignored. A later entry with the same id
 * replaces the earlier one. The `[explosion]` table takes the same sprite keys, for the bursts of a boss explosion.
 *
 * @param file Parsed `looks.toml`.
 * @param warnings Receives one message per ignored or corrected value.
 * @return The looks.
 */
[[nodiscard]] Looks readLooks(const config::ConfigFile& file, std::vector<std::string>& warnings);

}  // namespace client::looks
