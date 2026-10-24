#pragma once
#include <cstddef>
#include <functional>

#include "ecs/time/Time.hpp"

namespace ecs {
class Registry;
}

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

    /// Default maximum simulation steps allowed in a single tick call.
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
    std::size_t tick(float fixedDt, Step step);

    /**
     * @brief Advances the simulation by elapsed time, automatically updating the Time resource in the Registry.
     *
     * Invokes `step(Registry&)` once per whole `fixedDt` contained in the accumulated time.
     * The `ecs::time::Time` resource is stored or updated in `registry` before each step execution.
     *
     * @param fixedDt Duration of one simulation step in seconds (must be > 0).
     * @param registry ECS Registry containing or receiving the Time resource.
     * @param step Callable executed for each step, with signature `void(Registry&)`.
     * @return Number of steps executed.
     */
    template <typename Step>
    std::size_t tick(float fixedDt, Registry& registry, Step step);

    /**
     * @brief Time data of the latest executed step.
     */
    [[nodiscard]] const Time& time() const noexcept { return _time; }

    /**
     * @brief Real elapsed time in seconds between the two most recent tick calls.
     * @return Real frame delta time in seconds.
     */
    [[nodiscard]] float lastFrameDelta() const noexcept { return static_cast<float>(_lastElapsed); }

    /**
     * @brief Current frame timing data with variable real-time deltaTime.
     * @return Frame Time structure containing real-time deltaTime and current simulation progress.
     */
    [[nodiscard]] Time frameTime() const noexcept {
        return Time{
            .deltaTime = static_cast<float>(_lastElapsed),
            .totalElapsedTime = _time.totalElapsedTime,
            .tickCount = _time.tickCount,
        };
    }

    /**
     * @brief Restarts the clock: clears the accumulator and counters.
     */
    void reset();

  private:
    /** @brief Queries time source and computes seconds elapsed since last call. */
    double pollElapsed();

    /** @brief Monotonic time query callback. */
    TimeSource _source;

    /** @brief Upper bound of steps permitted per tick call. */
    std::size_t _maxStepsPerTick;

    /** @brief Timestamp of the previous poll instant. */
    double _lastInstant = 0.0;

    /** @brief Accumulated real time awaiting consumption in fixedDt steps. */
    double _accumulator = 0.0;

    /** @brief Real elapsed time in seconds measured during the most recent tick. */
    double _lastElapsed = 0.0;

    /** @brief Total accumulated elapsed simulation time. */
    double _totalElapsed = 0.0;

    /** @brief Exposed step time state. */
    Time _time;
};

}  // namespace ecs::time

#include "Clock.tpp"  // IWYU pragma: export
