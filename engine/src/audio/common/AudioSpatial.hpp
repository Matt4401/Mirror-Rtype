#pragma once

#include <algorithm>
#include <cmath>

#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioMiddleware.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/common/SpatialConfig.hpp"

namespace audio {

/** @brief Gain and stereo position of a sound, resulting from where it is heard from. */
struct SpatialGains {
    /** @brief Volume factor in [`MIN_VOLUME`, `MAX_VOLUME`], applied on top of the sound volume. */
    float volume = MAX_VOLUME;

    /** @brief Stereo position in [-`maxPan`, `maxPan`]. */
    float pan = PAN_CENTER;
};

/**
 * @brief Computes the stereo position of a sound from its horizontal offset to the listener.
 *
 * @param dx Emitter x minus listener x; positive values are on the right.
 * @param config Pan range and strength.
 * @return Pan in [-`maxPan`, `maxPan`]; `PAN_CENTER` for invalid inputs.
 */
[[nodiscard]] inline float computePan(float dx, const SpatialConfig& config) {
    if (!std::isfinite(dx) || !(config.panRange > 0.0F)) {
        return PAN_CENTER;
    }
    const float maxPan = clampFinite(config.maxPan, DEFAULT_MAX_PAN, PAN_CENTER, PAN_RIGHT);
    return std::clamp(dx / config.panRange, PAN_LEFT, PAN_RIGHT) * maxPan;
}

/**
 * @brief Computes how much a sound fades with its distance to the listener.
 *
 * Full volume up to `minDistance`, silent from `maxDistance`, linear in between.
 *
 * @param distance Distance between the emitter and the listener.
 * @param config Attenuation distances.
 * @return Volume factor in [`MIN_VOLUME`, `MAX_VOLUME`]; `MAX_VOLUME` for an invalid distance.
 */
[[nodiscard]] inline float computeAttenuation(float distance, const SpatialConfig& config) {
    if (!std::isfinite(distance) || distance <= config.minDistance) {
        return MAX_VOLUME;
    }
    if (distance >= config.maxDistance) {
        return MIN_VOLUME;
    }
    const float ratio = (config.maxDistance - distance) / (config.maxDistance - config.minDistance);
    return clampFinite(ratio, MAX_VOLUME, MIN_VOLUME, MAX_VOLUME);
}

/**
 * @brief Computes the gain and stereo position of a sound heard by a listener.
 *
 * A non-finite position is treated as a sound without position: full volume, centered.
 *
 * @param emitter Where the sound is emitted.
 * @param listener Where it is heard.
 * @param config Pan and attenuation settings.
 * @return The volume factor and pan to apply.
 */
[[nodiscard]] inline SpatialGains computeSpatial(SoundPosition emitter, SoundPosition listener,
                                                 const SpatialConfig& config) {
    const float dx = emitter.x - listener.x;
    const float dy = emitter.y - listener.y;
    return {.volume = computeAttenuation(std::hypot(dx, dy), config), .pan = computePan(dx, config)};
}

}  // namespace audio
