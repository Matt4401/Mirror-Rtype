#pragma once
#include <cmath>
#include <cstddef>
#include <utility>

#include "ecs/time/Clock.hpp"  // NOLINT(misc-header-include-cycle)

namespace ecs::time {

template <typename Step>
std::size_t Clock::tick(float fixedDt, Step&& step) {
    if (!(fixedDt > 0.0F)) {
        return 0;
    }

    const auto stepSeconds = static_cast<double>(fixedDt);
    _accumulator += pollElapsed();

    std::size_t steps = 0;
    while (_accumulator >= stepSeconds && steps < _maxStepsPerTick) {
        _accumulator -= stepSeconds;
        _totalElapsed += stepSeconds;
        _time.deltaTime = fixedDt;
        _time.totalElapsedTime = static_cast<float>(_totalElapsed);
        ++_time.tickCount;
        step(std::as_const(_time));
        ++steps;
    }

    if (_accumulator >= stepSeconds) {
        _accumulator = std::fmod(_accumulator, stepSeconds);
    }
    return steps;
}

}  // namespace ecs::time
