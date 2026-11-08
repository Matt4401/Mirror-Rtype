#include "metrics/ProcessUsage.hpp"

#include <cstdint>
#include <optional>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// psapi.h needs the types of windows.h: keep them in this order.
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#else
#include <sys/resource.h>
#include <unistd.h>

#include <fstream>
#endif

namespace metrics {

#ifdef _WIN32

namespace {

/** @brief Length of a FILETIME tick, in seconds. */
constexpr double FILETIME_TICK_SECONDS = 100e-9;

/** @brief Converts a FILETIME duration to seconds. */
double toSeconds(const FILETIME& time) {
    const std::uint64_t ticks = (static_cast<std::uint64_t>(time.dwHighDateTime) << 32U) | time.dwLowDateTime;
    return static_cast<double>(ticks) * FILETIME_TICK_SECONDS;
}

}  // namespace

std::optional<ProcessSample> sampleProcess() {
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    PROCESS_MEMORY_COUNTERS memory{};
    const HANDLE process = GetCurrentProcess();
    if (GetProcessTimes(process, &creation, &exit, &kernel, &user) == 0 ||
        GetProcessMemoryInfo(process, &memory, sizeof(memory)) == 0) {
        return std::nullopt;
    }
    return ProcessSample{
        .cpuSeconds = toSeconds(kernel) + toSeconds(user),
        .residentBytes = static_cast<std::uint64_t>(memory.WorkingSetSize),
    };
}

#else

namespace {

/** @brief Length of a microsecond, in seconds. */
constexpr double MICROSECOND_SECONDS = 1e-6;

/** @brief Converts a `timeval` duration to seconds. */
double toSeconds(const timeval& time) {  // NOLINT(misc-include-cleaner): POSIX defines it through sys/resource.h
    return static_cast<double>(time.tv_sec) + (static_cast<double>(time.tv_usec) * MICROSECOND_SECONDS);
}

}  // namespace

std::optional<ProcessSample> sampleProcess() {
    rusage usage{};  // NOLINT(misc-include-cleaner): POSIX defines it in sys/resource.h, glibc in a private header
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    // `ru_maxrss` is the peak, not the current value: only /proc gives the memory held right now.
    std::ifstream statm("/proc/self/statm");
    std::uint64_t totalPages = 0;
    std::uint64_t residentPages = 0;
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (!(statm >> totalPages >> residentPages) || pageSize <= 0) {
        return std::nullopt;
    }
    return ProcessSample{
        .cpuSeconds = toSeconds(usage.ru_utime) + toSeconds(usage.ru_stime),
        .residentBytes = residentPages * static_cast<std::uint64_t>(pageSize),
    };
}

#endif

}  // namespace metrics
