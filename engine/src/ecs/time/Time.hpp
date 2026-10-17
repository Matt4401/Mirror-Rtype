#pragma once
#include <cstdint>

namespace ecs::time {

/// Default simulation step: 60 Hz.
inline constexpr float DEFAULT_FIXED_DT = 1.0F / 60.0F;

/**
 * @brief Simulation time exposed to systems on each fixed tick.
 */
struct Time {
    float deltaTime = 0.0F;
    float totalElapsedTime = 0.0F;
    std::uint64_t tickCount = 0;
};

}  // namespace ecs::time
