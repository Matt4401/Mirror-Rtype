#pragma once

#include "audio/common/AudioConfig.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/common/SpatialConfig.hpp"

namespace audio {

/**
 * @brief Every audio setting a game can configure, usually read from its configuration file.
 *
 * `backend` and `voices` are used when the backend is created; volumes and `spatial` can change at any time.
 */
struct AudioSettings {
    /** @brief Audio library to use. */
    AudioBackendKind backend = AudioBackendKind::Auto;

    /** @brief Voice limits of the backend. */
    AudioConfig voices;

    /** @brief Pan and attenuation of positioned sounds. */
    SpatialConfig spatial;

    /** @brief Gain of the master bus. */
    float masterVolume = DEFAULT_VOLUME;

    /** @brief Gain of the music bus. */
    float musicVolume = DEFAULT_VOLUME;

    /** @brief Gain of the sound effect bus. */
    float sfxVolume = DEFAULT_VOLUME;
};

}  // namespace audio
