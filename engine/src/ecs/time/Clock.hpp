#pragma once
#include <cstddef>
#include <functional>

#include "ecs/time/Time.hpp"

namespace ecs::time {

/**
 * @brief Fixed-timestep simulation clock decoupled from the rendering framerate.
 *
 * Real elapsed time is accumulated, then consumed in constant `fixedDt` steps so that
 * simulation results do not depend on CPU speed. Has no graphical dependency, so it can
 * drive the headless server as well as the client.
 */
class Clock {
  public:
    /// Returns the current time in seconds, from an arbitrary but monotonic origin.
    using TimeSource = std::function<double()>;

    static constexpr std::size_t DEFAULT_MAX_STEPS_PER_TICK = 5;

    /**
     * @brief Creates a clock and records its starting instant.
     *
     * @param source Time source; defaults to `steadyNowSeconds`. Injectable for tests.
     * @param maxStepsPerTick Upper bound of steps run per `tick`, protecting against the "spiral of death".
     */
    explicit Clock(TimeSource source = nullptr, std::size_t maxStepsPerTick = DEFAULT_MAX_STEPS_PER_TICK);

    /**
     * @brief Advances the simulation by the real time elapsed since the previous call.
     *
     * Invokes `step(const Time&)` once per whole `fixedDt` contained in the accumulated time.
     * If more than `maxStepsPerTick` steps are due, the remaining backlog is dropped.
     *
     * @param fixedDt Duration of one simulation step in seconds (must be > 0).
     * @param step Callable executed for each step.
     * @return Number of steps executed.
     */
    template <typename Step>
    std::size_t tick(float fixedDt, Step&& step);

    /**
     * @brief Time data of the latest executed step.
     */
    [[nodiscard]] const Time& time() const noexcept { return _time; }

    /**
     * @brief Restarts the clock: clears the accumulator and counters.
     */
    void reset();

  private:
    double pollElapsed();

    TimeSource _source;
    std::size_t _maxStepsPerTick;
    double _lastInstant = 0.0;
    double _accumulator = 0.0;
    double _totalElapsed = 0.0;
    Time _time;
};

}  // namespace ecs::time

#include "Clock.tpp"  // IWYU pragma: export
