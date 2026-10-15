#pragma once
#include <cstdint>

namespace ecs::time {

/**
 * @brief Simulation time exposed to systems on each fixed tick.
 */
struct Time {
    float deltaTime = 0.0F;
    float totalElapsedTime = 0.0F;
    std::uint64_t tickCount = 0;
};

}  // namespace ecs::time
