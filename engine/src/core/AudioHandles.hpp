/**
 * @file AudioHandles.hpp
 * @brief Handle types of the audio module, owned by `engine_core` so that headless modules can name them.
 *
 * `audio/types/AudioTypes.hpp` and `audio/types/AudioConstants.hpp` re-export them; a module that only stores or
 * passes handles (the asset manager, components) includes this header and needs no audio library.
 */

#pragma once

#include <cstdint>

namespace audio {

/** @brief Handle of a loaded sound effect; `NO_SOUND` marks an invalid one. */
using SoundId = std::uint32_t;

/** @brief Handle of a loaded music stream; `NO_MUSIC` marks an invalid one. */
using MusicId = std::uint32_t;

/** @brief Handle of one playing instance of a sound; never reused, `NO_VOICE` marks an invalid one. */
using VoiceId = std::uint64_t;

/** @brief Value returned when a sound could not be loaded; playing it does nothing. */
inline constexpr SoundId NO_SOUND = 0;

/** @brief Value returned when a music stream could not be loaded; playing it does nothing. */
inline constexpr MusicId NO_MUSIC = 0;

/** @brief Value returned when a sound could not be started; updating or stopping it does nothing. */
inline constexpr VoiceId NO_VOICE = 0;

}  // namespace audio
