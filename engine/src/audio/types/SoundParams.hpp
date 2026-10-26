#pragma once

#include "audio/types/AudioConstants.hpp"

namespace audio {

/**
 * @brief Per-play settings of a sound effect, already resolved by the caller (bus volume, attenuation, panning).
 *
 * Out-of-range or non-finite values are clamped or replaced by their default by the backend.
 */
struct SoundParams {
    /** @brief Linear gain in [`MIN_VOLUME`, `MAX_VOLUME`]. */
    float volume = DEFAULT_VOLUME;

    /** @brief Stereo position in [`PAN_LEFT`, `PAN_RIGHT`]. */
    float pan = PAN_CENTER;

    /** @brief Playback speed factor in [`MIN_PITCH`, `MAX_PITCH`]. */
    float pitch = DEFAULT_PITCH;
};

}  // namespace audio
