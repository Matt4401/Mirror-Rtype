#include "ecs/time/PreciseSleep.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// mmsystem.h needs the types of windows.h, and WIN32_LEAN_AND_MEAN leaves it out of windows.h: keep them in this order.
// clang-format off
#include <windows.h>
#include <mmsystem.h>
// clang-format on
#endif

namespace ecs::time {

namespace {

#ifdef _WIN32

/** @brief Timer period asked for, in milliseconds. */
constexpr UINT TIMER_PERIOD_MS = 1;

/** @brief Asks for a 1 ms timer period. @return Whether Windows granted it. */
bool raiseTimerPeriod() noexcept { return timeBeginPeriod(TIMER_PERIOD_MS) == TIMERR_NOERROR; }

/** @brief Gives back the period granted by `raiseTimerPeriod`. */
void restoreTimerPeriod() noexcept { timeEndPeriod(TIMER_PERIOD_MS); }

#else

/** @brief Sleeps are already precise here: nothing to raise. @return Always `false`. */
bool raiseTimerPeriod() noexcept { return false; }

/** @brief Never called, since nothing was raised. */
void restoreTimerPeriod() noexcept {}

#endif

}  // namespace

PreciseSleepScope::PreciseSleepScope() noexcept : _raised(raiseTimerPeriod()) {}

PreciseSleepScope::~PreciseSleepScope() {
    if (_raised) {
        restoreTimerPeriod();
    }
}

}  // namespace ecs::time
