#pragma once

#include <memory>

#include "audio/IAudioBackend.hpp"
#include "audio/types/AudioConfig.hpp"
#include "audio/types/AudioTypes.hpp"

namespace audio {

/**
 * @brief Opens the requested audio output, falling back to a muted backend.
 *
 * `Auto` and `Raylib` return a raylib backend when an audio device could be opened, and a `NullAudioBackend`
 * otherwise (no sound card, CI, Docker), so callers never have to handle the missing-device case.
 * `Null` returns a `NullAudioBackend` without opening any device.
 * The audio device is global: keep a single backend alive at a time.
 *
 * @param kind Audio library to use.
 * @param config Voice limits.
 * @return A backend; never null.
 * @throws std::logic_error If a raylib backend already exists.
 */
[[nodiscard]] std::unique_ptr<IAudioBackend> makeAudioBackend(AudioBackendKind kind = AudioBackendKind::Auto,
                                                              AudioConfig config = AudioConfig{});

}  // namespace audio

// faudra add les autres icic
