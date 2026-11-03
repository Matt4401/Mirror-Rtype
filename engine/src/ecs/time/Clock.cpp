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

Time Clock::frameTime() const noexcept { return frameTime(static_cast<float>(_lastElapsed)); }

Time Clock::frameTime(float deltaTime) const noexcept {
    return Time{
        .deltaTime = deltaTime,
        .totalElapsedTime = _time.totalElapsedTime,
        .tickCount = _time.tickCount,
    };
}

void Clock::reset() {
    _lastInstant = _source();
    _accumulator = 0.0;
    _totalElapsed = 0.0;
    _lastElapsed = 0.0;
    _time = Time{
        .deltaTime = 0.0F,
        .totalElapsedTime = 0.0F,
        .tickCount = 0,
    };
}

void Clock::advance(float fixedDt) noexcept {
    const auto stepSeconds = static_cast<double>(fixedDt);
    _totalElapsed += stepSeconds;
    _time.deltaTime = fixedDt;
    _time.totalElapsedTime = static_cast<float>(_totalElapsed);
    ++_time.tickCount;
}

double Clock::pollElapsed() {
    const double now = _source();
    const double elapsed = std::max(now - _lastInstant, 0.0);
    _lastInstant = now;
    return elapsed;
}

}  // namespace ecs::time
