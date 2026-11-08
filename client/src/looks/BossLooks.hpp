/**
 * @file BossLooks.hpp
 * @brief How the client draws each boss variant and each boss part look, read from `client/config/bosses.toml`.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "config/ConfigFile.hpp"

namespace client::looks {

/** @brief Path of the boss looks file, from the directory the client runs from. */
inline constexpr const char* BOSS_LOOKS_PATH = "client/config/bosses.toml";
/** @brief Most `[[boss]]` or `[[part]]` entries read; the next ones are ignored, with a warning. */
inline constexpr std::size_t MAX_LOOKS = 64;
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

/** @brief Every configured look: bosses by `variant`, parts by `look`. A missing entry draws the default box. */
struct BossLooks {
    /** @brief Looks of the boss variants. */
    std::map<std::uint8_t, Look> bosses;
    /** @brief Looks of the boss parts. */
    std::map<std::uint8_t, Look> parts;
    /** @brief Sprite of one explosion burst (`[explosion]`), played once; without it, bursts are growing boxes. */
    std::optional<SpriteLook> explosion;
};

/**
 * @brief Reads the boss looks of a configuration file.
 *
 * Reads the `[[boss]]` entries (key `variant`) and the `[[part]]` entries (key `look`). Each entry sets `width` and
 * `height` (box drawn without a sprite) and, optionally, a sprite: `texture`, `frame_x`, `frame_y`, `frame_width`,
 * `frame_height`, `frames`, `frame_duration`, `loop` and `scale`. Out-of-range values are clamped; an entry without its
 * id, a sprite without its frame size, an unsafe texture path (absolute, with `..` or `\`) and unknown keys are
 * reported in `warnings` and ignored. A later entry with the same id replaces the earlier one. The `[explosion]` table
 * takes the same sprite keys, for the bursts of a boss explosion.
 *
 * @param file Parsed `bosses.toml`.
 * @param warnings Receives one message per ignored or corrected value.
 * @return The looks.
 */
[[nodiscard]] BossLooks readBossLooks(const config::ConfigFile& file, std::vector<std::string>& warnings);

}  // namespace client::looks
