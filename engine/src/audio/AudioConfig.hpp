#pragma once

#include <cstddef>

#include "audio/AudioConstants.hpp"

namespace audio {

/**
 * @brief Voice limits of an audio backend.
 *
 * Bounding the voices keeps the mixer cost constant when many sounds are triggered at once.
 */
struct AudioConfig {
    /** @brief Number of instances of a single sound that can play at the same time (at least 1). */
    std::size_t voicesPerSound = DEFAULT_VOICES_PER_SOUND;

    /** @brief Number of sound instances that can play at the same time across all sounds (at least 1). */
    std::size_t maxActiveVoices = DEFAULT_MAX_ACTIVE_VOICES;
};

}  // namespace audio
