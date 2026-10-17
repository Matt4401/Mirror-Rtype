#include "ecs/time/Clock.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "ecs/time/SteadyNow.hpp"
#include "ecs/time/Time.hpp"

namespace ecs::time {

Clock::Clock(TimeSource source, std::size_t maxStepsPerTick)
    : _source(source ? std::move(source) : TimeSource(steadyNowSeconds)),
      _maxStepsPerTick(std::max<std::size_t>(maxStepsPerTick, 1)),
      _lastInstant(_source()) {}

void Clock::reset() {
    _lastInstant = _source();
    _accumulator = 0.0;
    _totalElapsed = 0.0;
    _time = Time{};
}

double Clock::pollElapsed() {
    const double now = _source();
    const double elapsed = std::max(now - _lastInstant, 0.0);
    _lastInstant = now;
    return elapsed;
}

}  // namespace ecs::time
