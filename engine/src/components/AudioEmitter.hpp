#pragma once

#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"

namespace ecs {

/**
 * @brief Makes an entity emit a sound that follows its `Transform` while it plays (engine hum, moving enemy, ...).
 *
 * The audio system starts the sound, then updates its pan and attenuation every frame from the entity position.
 * The sound stops when the component or the entity is removed, or when `playing` is set to false.
 * For one-off sounds that do not move (shots, explosions), publish a `PlaySoundEvent` instead.
 */
struct AudioEmitter {
    /** @brief Sound to emit; `NO_SOUND` emits nothing. */
    audio::SoundId sound = audio::NO_SOUND;

    /** @brief Gain of the sound, multiplied by the SFX bus volume and the distance attenuation. */
    float volume = audio::DEFAULT_VOLUME;

    /** @brief Playback speed factor (1.0F keeps the original pitch). */
    float pitch = audio::DEFAULT_PITCH;

    /** @brief Whether the sound restarts when it reaches its end. */
    bool loop = true;

    /** @brief Set to false to stop the sound; setting it back to true starts it again. */
    bool playing = true;
};

}  // namespace ecs
