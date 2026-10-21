#pragma once

#include <memory>

#include "audio/IAudioBackend.hpp"
#include "audio/common/AudioConfig.hpp"

namespace audio {

/**
 * @brief Opens the audio output of the platform, falling back to a muted backend.
 *
 * Returns a raylib backend when an audio device could be opened, and a `NullAudioBackend` otherwise
 * (no sound card, CI, Docker), so callers never have to handle the missing-device case.
 * The audio device is global: keep a single backend alive at a time.
 *
 * @param config Voice limits.
 * @return A backend; never null.
 * @throws std::logic_error If a raylib backend already exists.
 */
[[nodiscard]] std::unique_ptr<IAudioBackend> makeAudioBackend(AudioConfig config = AudioConfig{});

}  // namespace audio

// faudra add les autres icic
