#pragma once

namespace ecs::time {

/**
 * @brief Makes short sleeps last about what they ask for, for as long as the scope lives.
 *
 * On Windows, a sleep lasts at least one period of the system timer, about 15.6 ms by default: a loop that sleeps
 * 1 ms between two `Engine::advance()` calls really wakes up every 15.6 ms, which delays the fixed steps and makes them
 * come in bursts. This scope asks Windows for a 1 ms period (`timeBeginPeriod(1)`) and gives it back when destroyed.
 * Linux and macOS already sleep with a sub-millisecond precision: there the scope does nothing.
 *
 * Create one at the start of a program whose loop sleeps, such as a headless server. A raylib window already does the
 * same, so a windowed client does not need one.
 */
class PreciseSleepScope {
  public:
    /** @brief Asks the system for a 1 ms timer period, on Windows only. */
    PreciseSleepScope() noexcept;

    /** @brief Gives the timer period back, if the constructor raised it. */
    ~PreciseSleepScope();

    PreciseSleepScope(const PreciseSleepScope&) = delete;
    PreciseSleepScope& operator=(const PreciseSleepScope&) = delete;
    PreciseSleepScope(PreciseSleepScope&&) = delete;
    PreciseSleepScope& operator=(PreciseSleepScope&&) = delete;

    /**
     * @brief Whether this scope changed the system timer period.
     * @return `true` on Windows once the 1 ms period is granted, always `false` elsewhere (nothing to change).
     */
    [[nodiscard]] bool raised() const noexcept { return _raised; }

  private:
    /** @brief The 1 ms period was granted, so the destructor must give it back. */
    bool _raised = false;
};

}  // namespace ecs::time
