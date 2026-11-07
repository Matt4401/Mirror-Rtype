#include "metrics/MetricsPlugin.hpp"

#include <format>

#include "core/Engine.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/ProcessUsage.hpp"

namespace metrics {

namespace {

// Below the usual 60 FPS target: a game capped at 60 FPS averages 59.x and must not show a warning all the time.
constexpr float FPS_WARNING = 55.0F;
constexpr float FPS_CRITICAL = 30.0F;
constexpr float MILLISECONDS_PER_SECOND = 1000.0F;
constexpr double BYTES_PER_MEGABYTE = 1024.0 * 1024.0;
constexpr double PERCENT = 100.0;

}  // namespace

MetricLevel frameLevel(const float averageSeconds) noexcept {
    if (averageSeconds * FPS_CRITICAL > 1.0F) {
        return MetricLevel::Critical;
    }
    return averageSeconds * FPS_WARNING > 1.0F ? MetricLevel::Warning : MetricLevel::Normal;
}

void MetricsPlugin::init(engine::Engine& engine) {
    auto& registry = engine.registry();
    if (!registry.hasResource<MetricsBoard>()) {
        registry.emplaceResource<MetricsBoard>();
    }
    _lastProcess = sampleProcess();
}

void MetricsPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    _frames.push(time.deltaTime);
    if (const auto elapsed = _timer.tick(time.deltaTime)) {
        publish(engine.registry(), *elapsed);
    }
}

void MetricsPlugin::publish(ecs::Registry& registry, const float elapsed) {
    auto& board = registry.getResource<MetricsBoard>();

    const float average = _frames.average();
    if (average > 0.0F) {
        const MetricLevel level = frameLevel(average);
        board.set("FPS", std::format("{:.0f}", 1.0F / average), level);
        board.set("Frame",
                  std::format("{:.1f} ms avg, {:.1f} ms max", average * MILLISECONDS_PER_SECOND,
                              _frames.max() * MILLISECONDS_PER_SECOND),
                  level);
    }
    board.set("Entities", std::format("{}", registry.aliveCount()));

    const auto process = sampleProcess();
    if (process && _lastProcess) {
        const double used = process->cpuSeconds - _lastProcess->cpuSeconds;
        board.set("CPU", std::format("{:.0f} %", used / static_cast<double>(elapsed) * PERCENT));
    }
    if (process) {
        board.set("RAM", std::format("{:.1f} MB", static_cast<double>(process->residentBytes) / BYTES_PER_MEGABYTE));
    }
    _lastProcess = process;
}

}  // namespace metrics
