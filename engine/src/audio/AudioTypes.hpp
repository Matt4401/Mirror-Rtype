#pragma once

#include <cstddef>
#include <cstdint>

namespace audio {

/** @brief Handle of a loaded sound effect. */
using SoundId = std::uint32_t;

/** @brief Value returned when a sound could not be loaded; playing it does nothing. */
inline constexpr SoundId NO_SOUND = 0;

/** @brief Handle of a loaded music stream. */
using MusicId = std::uint32_t;

/** @brief Value returned when a music stream could not be loaded; playing it does nothing. */
inline constexpr MusicId NO_MUSIC = 0;

/**
 * @brief Per-play settings of a sound effect, already resolved by the caller (bus volume, attenuation, panning).
 *
 * Out-of-range or non-finite values are clamped or replaced by their default by the backend.
 */
struct SoundParams {
    /** @brief Linear gain in [0.0F, 1.0F]. */
    float volume = 1.0F;

    /** @brief Stereo position in [-1.0F, 1.0F]: -1 is full left, 0 is center, 1 is full right. */
    float pan = 0.0F;

    /** @brief Playback speed factor (1.0F keeps the original pitch). */
    float pitch = 1.0F;
};

/**
 * @brief Voice limits of an audio backend.
 *
 * Bounding the voices keeps the mixer cost constant when many sounds are triggered at once.
 */
struct AudioConfig {
    /** @brief Number of instances of a single sound that can play at the same time (at least 1). */
    std::size_t voicesPerSound = 4;

    /** @brief Number of sound instances that can play at the same time across all sounds (at least 1). */
    std::size_t maxActiveVoices = 32;
};

}  // namespace audio
