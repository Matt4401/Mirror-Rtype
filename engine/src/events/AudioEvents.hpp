#pragma once

#include <optional>

#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"

namespace events {

/**
 * @brief Requests a sound effect.
 *
 * Published by gameplay code, which never touches the audio library: the audio system plays it on its next update.
 * Requesting the same sound several times before that update plays only the loudest request.
 */
struct PlaySoundEvent {
    /** @brief Sound to play; `NO_SOUND` is ignored. */
    audio::SoundId sound = audio::NO_SOUND;

    /** @brief Gain of this instance, multiplied by the SFX bus volume. */
    float volume = audio::DEFAULT_VOLUME;

    /** @brief Playback speed factor (1.0F keeps the original pitch). */
    float pitch = audio::DEFAULT_PITCH;

    /**
     * @brief World position of the emitter, to pan and attenuate the sound around the `AudioListener`.
     *
     * Leave it empty for sounds that have no place in the world (UI, the player's own feedback).
     */
    std::optional<audio::SoundPosition> position;
};

/** @brief Requests a music stream, replacing the current one. */
struct PlayMusicEvent {
    /** @brief Music to play; `NO_MUSIC` is ignored. */
    audio::MusicId music = audio::NO_MUSIC;

    /** @brief Whether the music restarts when it reaches its end. */
    bool loop = true;
};

/** @brief Requests the current music stream to stop. */
struct StopMusicEvent {};

/** @brief Requests every playing sound effect to stop (pause menu, scene change, ...). */
struct StopAllSoundsEvent {};

/** @brief Changes the gain of a volume bus (options menu, console, ...). */
struct SetAudioVolumeEvent {
    /** @brief Bus to change. */
    audio::AudioBus bus = audio::AudioBus::Master;

    /** @brief New linear gain in [`MIN_VOLUME`, `MAX_VOLUME`]. */
    float volume = audio::DEFAULT_VOLUME;
};

}  // namespace events
