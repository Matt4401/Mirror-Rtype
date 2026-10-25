#pragma once

#include <cstdint>

namespace audio {

/** @brief Handle of a loaded sound effect; `NO_SOUND` marks an invalid one. */
using SoundId = std::uint32_t;

/** @brief Handle of a loaded music stream; `NO_MUSIC` marks an invalid one. */
using MusicId = std::uint32_t;

/** @brief Handle of one playing instance of a sound; never reused, `NO_VOICE` marks an invalid one. */
using VoiceId = std::uint64_t;

/** @brief What became of a playing instance of a sound. */
enum class VoiceState : std::uint8_t {
    /** @brief Still playing. */
    Playing,
    /** @brief Reached its end, was stopped, or was never started. */
    Ended,
    /** @brief Cut short because its voice was taken by another instance when a voice limit was reached. */
    Stolen,
};

/** @brief Point in world space where a sound is emitted or heard. */
struct SoundPosition {
    /** @brief Horizontal position in world units. */
    float x = 0.0F;

    /** @brief Vertical position in world units. */
    float y = 0.0F;
};

/** @brief Audio library used to output sound. */
enum class AudioBackendKind : std::uint8_t {
    /** @brief The best library available, falling back to a muted output. */
    Auto,
    /** @brief raylib (miniaudio), falling back to a muted output if no audio device can be opened. */
    Raylib,
    /** @brief Muted output: no audio device is opened and nothing is played. */
    Null,
};

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
