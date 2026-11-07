/**
 * @file MetricsPlugin.hpp
 * @brief Plugin measuring the frame rate and the resources of the process.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "ecs/Registry.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/ProcessUsage.hpp"
#include "metrics/RollingStats.hpp"

namespace ecs::time {
struct Time;
}

namespace metrics {

/**
 * @brief How worrying an average frame time is.
 * @param averageSeconds Average frame time in seconds.
 * @return `Normal` from 55 FPS on, `Warning` below, `Critical` below 30 FPS.
 */
[[nodiscard]] MetricLevel frameLevel(float averageSeconds) noexcept;

/**
 * @brief Creates the `MetricsBoard` resource and keeps its `FPS`, `Frame`, `Entities`, `CPU` and `RAM` lines up to
 * date.
 *
 * It draws nothing and needs no window, so a headless program can use it too. Other plugins add their own lines to the
 * board: see `ecs::net::NetworkMetricsPlugin`.
 */
class MetricsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "MetricsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "MetricsPlugin"; }

    /**
     * @brief Creates the `MetricsBoard` resource.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Records the frame time and refreshes the board every `REFRESH_PERIOD` seconds.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Gets the last frame times.
     * @return Frame times, in seconds.
     */
    [[nodiscard]] const RollingStats& frames() const noexcept { return _frames; }

  private:
    /**
     * @brief Writes every metric of this plugin to the board.
     * @param registry Registry holding the board.
     * @param elapsed Seconds since the last refresh.
     */
    void publish(ecs::Registry& registry, float elapsed);

    /** @brief Last frame times, in seconds. */
    RollingStats _frames;
    /** @brief Paces the refreshes. */
    RefreshTimer _timer;
    /** @brief Measure of the process at the last refresh, to compute the CPU used since then. */
    std::optional<ProcessSample> _lastProcess;
};

}  // namespace metrics
