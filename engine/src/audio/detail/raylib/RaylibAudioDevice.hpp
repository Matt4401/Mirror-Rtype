#pragma once

#include <atomic>

namespace audio::detail {

/**
 * @brief Flag set while a `RaylibAudioBackend` owns raylib's global audio device.
 *
 * raylib has a single audio device for the whole process: closing it from one backend would mute any other.
 * The function is `inline` so every translation unit shares the same flag.
 *
 * @return The process-wide flag.
 */
inline std::atomic_bool& deviceClaimed() {
    static std::atomic_bool claimed{false};
    return claimed;
}

}  // namespace audio::detail
