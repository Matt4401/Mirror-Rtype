#pragma once

#include <cstddef>

#include "audio/types/AudioTypes.hpp"

namespace audio {

/** @brief Value returned when a sound could not be loaded; playing it does nothing. */
inline constexpr SoundId NO_SOUND = 0;

/** @brief Value returned when a music stream could not be loaded; playing it does nothing. */
inline constexpr MusicId NO_MUSIC = 0;

/** @brief Value returned when a sound could not be started; updating or stopping it does nothing. */
inline constexpr VoiceId NO_VOICE = 0;

/** @brief Silent gain. */
inline constexpr float MIN_VOLUME = 0.0F;

/** @brief Full gain; louder values are clamped to it. */
inline constexpr float MAX_VOLUME = 1.0F;

/** @brief Gain used when none is given. */
inline constexpr float DEFAULT_VOLUME = 1.0F;

/** @brief Stereo position of a sound heard only in the left speaker. */
inline constexpr float PAN_LEFT = -1.0F;

/** @brief Stereo position of a sound heard equally in both speakers. */
inline constexpr float PAN_CENTER = 0.0F;

/** @brief Stereo position of a sound heard only in the right speaker. */
inline constexpr float PAN_RIGHT = 1.0F;

/** @brief Slowest playback speed factor; lower values are clamped to it. */
inline constexpr float MIN_PITCH = 0.1F;

/** @brief Fastest playback speed factor; higher values are clamped to it. */
inline constexpr float MAX_PITCH = 4.0F;

/** @brief Playback speed factor keeping the original pitch. */
inline constexpr float DEFAULT_PITCH = 1.0F;

/** @brief Default number of instances of a single sound that can play at the same time. */
inline constexpr std::size_t DEFAULT_VOICES_PER_SOUND = 4;

/** @brief Default number of sound instances that can play at the same time across all sounds. */
inline constexpr std::size_t DEFAULT_MAX_ACTIVE_VOICES = 32;

/** @brief Largest accepted `voicesPerSound`, so a configuration cannot allocate thousands of voices per sound. */
inline constexpr std::size_t VOICES_PER_SOUND_LIMIT = 16;

/** @brief Largest accepted `maxActiveVoices`. */
inline constexpr std::size_t ACTIVE_VOICES_LIMIT = 256;

/** @brief Sound requests kept between two audio updates; extra requests are dropped. */
inline constexpr std::size_t MAX_QUEUED_SOUNDS = 128;

/** @brief Default horizontal distance to the listener, in world units, at which a sound reaches its widest pan. */
inline constexpr float DEFAULT_PAN_RANGE = 640.0F;

/** @brief Default widest pan of positioned sounds; fully one-sided sounds tire the ear on headphones. */
inline constexpr float DEFAULT_MAX_PAN = 0.6F;

/** @brief Default distance to the listener, in world units, under which positioned sounds keep their full volume. */
inline constexpr float DEFAULT_MIN_DISTANCE = 750.0F;

/** @brief Default distance to the listener, in world units, from which positioned sounds are silent. */
inline constexpr float DEFAULT_MAX_DISTANCE = 1500.0F;

/** @brief Default distance, in world units, under which requests of the same sound in one update are merged. */
inline constexpr float DEFAULT_MERGE_DISTANCE = 32.0F;

}  // namespace audio
