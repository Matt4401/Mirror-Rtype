/**
 * @file ProcessUsage.hpp
 * @brief Resources used by the current process, behind one function for every platform.
 */

#pragma once

#include <cstdint>
#include <optional>

namespace metrics {

/** @brief What the current process has used so far. */
struct ProcessSample {
    /** @brief CPU time used by all its threads since it started, in seconds. */
    double cpuSeconds = 0.0;
    /** @brief Physical memory it currently holds, in bytes. */
    std::uint64_t residentBytes = 0;
};

/**
 * @brief Measures the current process.
 * @return The measure, or nothing if the platform does not provide it.
 */
[[nodiscard]] std::optional<ProcessSample> sampleProcess();

}  // namespace metrics
