#pragma once

#include <cstdint>

namespace audio {

/** @brief Handle of a loaded sound effect; `NO_SOUND` marks an invalid one. */
using SoundId = std::uint32_t;

/** @brief Handle of a loaded music stream; `NO_MUSIC` marks an invalid one. */
using MusicId = std::uint32_t;

/** @brief Volume channel; the gain of a sound is the product of its own volume and of its buses. */
enum class AudioBus : std::uint8_t {
    /** @brief Global gain applied to every sound and music. */
    Master,
    /** @brief Gain of music streams. */
    Music,
    /** @brief Gain of sound effects. */
    Sfx,
};

}  // namespace audio
