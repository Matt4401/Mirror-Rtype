#pragma once

#include <raylib.h>

#include "audio/AudioMiddleware.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/SoundParams.hpp"

namespace audio::detail {

/**
 * @brief Applies volume, pan and pitch to a raylib sound, clamping invalid values.
 *
 * @param sound raylib sound or alias to configure.
 * @param params Settings to apply; NaN, infinite or out-of-range values are replaced or clamped.
 */
inline void applyParams(const Sound& sound, const SoundParams& params) {
    SetSoundVolume(sound, clampFinite(params.volume, DEFAULT_VOLUME, MIN_VOLUME, MAX_VOLUME));
    SetSoundPan(sound, clampFinite(params.pan, PAN_CENTER, PAN_LEFT, PAN_RIGHT));
    SetSoundPitch(sound, clampFinite(params.pitch, DEFAULT_PITCH, MIN_PITCH, MAX_PITCH));
}

}  // namespace audio::detail
