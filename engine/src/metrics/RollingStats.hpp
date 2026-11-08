/**
 * @file RollingStats.hpp
 * @brief Average, minimum and maximum of the last samples of a value, in constant memory.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <span>

namespace metrics {

/** @brief Keeps the last `CAPACITY` samples of a value: pushing never allocates. */
class RollingStats {
  public:
    /** @brief Number of samples kept: 2 seconds at 60 FPS. */
    static constexpr std::size_t CAPACITY = 120;

    /**
     * @brief Adds a sample, replacing the oldest one once `CAPACITY` samples are kept.
     * @param sample The sample.
     */
    void push(const float sample) noexcept {
        _samples.at(_next) = sample;
        _next = (_next + 1) % CAPACITY;
        _count = std::min(_count + 1, CAPACITY);
    }

    /**
     * @brief Tells if there is no sample yet.
     * @return True before the first `push`.
     */
    [[nodiscard]] bool empty() const noexcept { return _count == 0; }

    /**
     * @brief Computes the average of the samples.
     * @return The average, 0 without samples.
     */
    [[nodiscard]] float average() const noexcept {
        if (empty()) {
            return 0.0F;
        }
        const auto samples = kept();
        return std::accumulate(samples.begin(), samples.end(), 0.0F) / static_cast<float>(_count);
    }

    /**
     * @brief Finds the smallest sample.
     * @return The smallest sample, 0 without samples.
     */
    [[nodiscard]] float min() const noexcept { return empty() ? 0.0F : std::ranges::min(kept()); }

    /**
     * @brief Finds the largest sample.
     * @return The largest sample, 0 without samples.
     */
    [[nodiscard]] float max() const noexcept { return empty() ? 0.0F : std::ranges::max(kept()); }

  private:
    /**
     * @brief Gets the samples kept, in no particular order.
     * @return The samples.
     */
    [[nodiscard]] std::span<const float> kept() const noexcept { return std::span(_samples).first(_count); }

    /** @brief Samples; only the first `_count` are meaningful. */
    std::array<float, CAPACITY> _samples{};
    /** @brief Where the next sample goes. */
    std::size_t _next = 0;
    /** @brief Number of samples kept. */
    std::size_t _count = 0;
};

}  // namespace metrics
