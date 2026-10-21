#pragma once

#include <cstddef>

#include "audio/common/AudioTypes.hpp"

namespace audio {

/** @brief Value returned when a sound could not be loaded; playing it does nothing. */
inline constexpr SoundId NO_SOUND = 0;

/** @brief Value returned when a music stream could not be loaded; playing it does nothing. */
inline constexpr MusicId NO_MUSIC = 0;

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

}  // namespace audio
