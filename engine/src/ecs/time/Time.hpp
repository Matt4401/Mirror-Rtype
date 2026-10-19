#pragma once
#include <cstdint>

namespace ecs::time {

/// Default simulation step: 60 Hz.
inline constexpr float DEFAULT_FIXED_DT = 1.0F / 60.0F;

/**
 * @brief Simulation time exposed to systems on each fixed tick.
 */
struct Time {
    /** @brief Duration of the current fixed tick step in seconds. */
    float deltaTime = 0.0F;

    /** @brief Cumulative elapsed simulation time in seconds. */
    float totalElapsedTime = 0.0F;

    /** @brief Number of fixed ticks executed since simulation start. */
    std::uint64_t tickCount = 0;
};

}  // namespace ecs::time
