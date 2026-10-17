#include "ecs/time/SteadyNow.hpp"

#include <chrono>

namespace ecs::time {

double steadyNowSeconds() {
    using SecondsF = std::chrono::duration<double>;
    return std::chrono::duration_cast<SecondsF>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace ecs::time
