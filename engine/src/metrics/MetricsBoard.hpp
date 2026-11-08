/**
 * @file MetricsBoard.hpp
 * @brief Registry resource listing the metrics to show, whoever measures them and however they are shown.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace metrics {

/** @brief Seconds between two refreshes of a metric: often enough to follow it, slowly enough to read it. */
inline constexpr float REFRESH_PERIOD = 0.5F;

/** @brief How worrying the value of a metric is. */
enum class MetricLevel : std::uint8_t {
    /** @brief Nothing to report. */
    Normal,
    /** @brief The value is getting worse than it should be. */
    Warning,
    /** @brief The value is bad enough to be felt by the player. */
    Critical,
};

/** @brief One named value of the board. */
struct Metric {
    /** @brief Name of the metric, unique in the board. */
    std::string name;
    /** @brief Value, already formatted with its unit. */
    std::string value;
    /** @brief How worrying the value is. */
    MetricLevel level = MetricLevel::Normal;
};

/**
 * @brief Ordered list of metrics: producers write to it, displays read it.
 *
 * A producer never knows who displays its metrics, and a display never knows who measures them, so a game adds its own
 * metrics with a single `set`.
 */
class MetricsBoard {
  public:
    /**
     * @brief Updates a metric, or adds it at the end of the board if it is new: lines never change order.
     * @param name Name of the metric.
     * @param value Value, formatted with its unit.
     * @param level How worrying the value is.
     */
    void set(std::string_view name, std::string value, MetricLevel level = MetricLevel::Normal);

    /**
     * @brief Gets the metrics.
     * @return The metrics, in the order they were first set.
     */
    [[nodiscard]] const std::vector<Metric>& entries() const noexcept { return _entries; }

    /**
     * @brief Looks a metric up.
     * @param name Name of the metric.
     * @return The metric, or nullptr if it was never set.
     */
    [[nodiscard]] const Metric* find(std::string_view name) const noexcept;

    /**
     * @brief Gets a number that changes each time the content of the board changes.
     * @return The version: a display only has to redraw its text when it differs from the last one it read.
     */
    [[nodiscard]] std::uint64_t version() const noexcept { return _version; }

  private:
    /** @brief Metrics, in the order they were first set. */
    std::vector<Metric> _entries;
    /** @brief Number of changes so far. */
    std::uint64_t _version = 0;
};

/** @brief Tells a producer when to refresh its metrics, from the time that elapsed and not from a number of frames. */
class RefreshTimer {
  public:
    /**
     * @brief Lets time elapse.
     * @param deltaTime Seconds elapsed since the last call.
     * @return Seconds elapsed since the last refresh if a refresh is due now, nothing otherwise.
     */
    [[nodiscard]] std::optional<float> tick(float deltaTime) noexcept {
        _elapsed += deltaTime;
        if (_elapsed < REFRESH_PERIOD) {
            return std::nullopt;
        }
        const float elapsed = _elapsed;
        _elapsed = 0.0F;
        return elapsed;
    }

  private:
    /** @brief Seconds elapsed since the last refresh. */
    float _elapsed = 0.0F;
};

}  // namespace metrics
