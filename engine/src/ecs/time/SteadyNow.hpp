#pragma once

namespace ecs::time {

/**
 * @brief Current time of the monotonic system clock, in seconds from an arbitrary origin.
 */
double steadyNowSeconds();

}  // namespace ecs::time
