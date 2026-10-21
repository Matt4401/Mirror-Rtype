#pragma once

#include <algorithm>
#include <cmath>

namespace audio {

/**
 * @brief Clamps an untrusted value, so NaN and infinities never reach the audio library.
 *
 * @param value Value to check.
 * @param fallback Value returned when `value` is NaN or infinite.
 * @param min Lower bound.
 * @param max Upper bound.
 * @return `value` clamped to [min, max], or `fallback` if it is not finite.
 */
[[nodiscard]] inline float clampFinite(float value, float fallback, float min, float max) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, min, max);
}

}  // namespace audio
