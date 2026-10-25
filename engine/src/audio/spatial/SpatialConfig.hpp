#pragma once

#include "audio/types/AudioConstants.hpp"

namespace audio {

/**
 * @brief How positioned sounds are panned and attenuated around the listener.
 *
 * Distances are in world units. The defaults suit a 1280x720 view centered on the listener: every visible sound
 * keeps its full volume, and only off-screen sounds fade out.
 */
struct SpatialConfig {
    /** @brief Whether positioned sounds are panned and attenuated; when false they all play centered. */
    bool enabled = true;

    /** @brief Horizontal distance at which a sound reaches `maxPan`; closer sounds are panned proportionally. */
    float panRange = DEFAULT_PAN_RANGE;

    /** @brief Widest pan, in [0.0F, 1.0F]: 1 sends a far-left sound to the left speaker only. */
    float maxPan = DEFAULT_MAX_PAN;

    /** @brief Distance under which a sound keeps its full volume. */
    float minDistance = DEFAULT_MIN_DISTANCE;

    /** @brief Distance from which a sound is silent; the volume falls linearly in between. */
    float maxDistance = DEFAULT_MAX_DISTANCE;

    /**
     * @brief Requests of the same sound in one update closer than this are played once, at the loudest request.
     *
     * Farther requests are all played, each from its own position (two players firing on the same frame).
     */
    float mergeDistance = DEFAULT_MERGE_DISTANCE;
};

}  // namespace audio
