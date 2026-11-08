#include "net/metrics/NetworkMetricsPlugin.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkManager.hpp"

namespace ecs::net {

namespace {

constexpr float BYTES_PER_KILOBYTE = 1000.0F;
constexpr float RTT_WARNING_MS = 100.0F;
constexpr float RTT_CRITICAL_MS = 250.0F;
constexpr float MISSED_WARNING_PERCENT = 1.0F;
constexpr float MISSED_CRITICAL_PERCENT = 10.0F;
constexpr float PERCENT = 100.0F;

/** @brief Gets the level of a value that gets worse as it grows. */
metrics::MetricLevel levelOf(const float value, const float warning, const float critical) {
    if (value > critical) {
        return metrics::MetricLevel::Critical;
    }
    return value > warning ? metrics::MetricLevel::Warning : metrics::MetricLevel::Normal;
}

/** @brief Formats a rate from a counter increase. */
float perSecond(const std::uint64_t increase, const float elapsed) { return static_cast<float>(increase) / elapsed; }

}  // namespace

NetworkMetricsPlugin::NetworkMetricsPlugin(std::shared_ptr<const INetworkManager> transport)
    : _transport(std::move(transport)) {}

void NetworkMetricsPlugin::build(engine::PluginBuilder& builder) { builder.require<metrics::MetricsPlugin>(); }

void NetworkMetricsPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    const auto elapsed = _timer.tick(time.deltaTime);
    if (!elapsed) {
        return;
    }
    auto& registry = engine.registry();
    auto& board = registry.getResource<metrics::MetricsBoard>();
    publishTraffic(board, *elapsed);
    if (registry.hasResource<ReplicationStats>()) {
        publishReplication(board, registry.getResource<ReplicationStats>());
    }
}

void NetworkMetricsPlugin::publishTraffic(metrics::MetricsBoard& board, const float elapsed) {
    const TransportStats traffic = _transport->stats();
    const float bytesIn = perSecond(traffic.bytesReceived - _lastTraffic.bytesReceived, elapsed);
    const float bytesOut = perSecond(traffic.bytesSent - _lastTraffic.bytesSent, elapsed);
    const float packetsIn = perSecond(traffic.packetsReceived - _lastTraffic.packetsReceived, elapsed);
    const float packetsOut = perSecond(traffic.packetsSent - _lastTraffic.packetsSent, elapsed);
    _lastTraffic = traffic;

    board.set("Down", std::format("{:.1f} KB/s", bytesIn / BYTES_PER_KILOBYTE));
    board.set("Up", std::format("{:.1f} KB/s", bytesOut / BYTES_PER_KILOBYTE));
    board.set("Packets", std::format("{:.0f} in, {:.0f} out /s", packetsIn, packetsOut));
}

void NetworkMetricsPlugin::publishReplication(metrics::MetricsBoard& board, const ReplicationStats& stats) {
    if (stats.inputRttMs) {
        board.set("RTT", std::format("{:.0f} ms", *stats.inputRttMs),
                  levelOf(*stats.inputRttMs, RTT_WARNING_MS, RTT_CRITICAL_MS));
    } else {
        board.set("RTT", "--");
    }

    // A snapshot that arrives late was counted as missed when the next one came: do not count it twice.
    const std::uint64_t jumped = stats.snapshotsMissed - _lastReplication.snapshotsMissed;
    const std::uint64_t late = stats.snapshotsStale - _lastReplication.snapshotsStale;
    const std::uint64_t missed = jumped - std::min(jumped, late);
    const std::uint64_t expected = (stats.snapshotsReceived - _lastReplication.snapshotsReceived) + missed;
    _lastReplication = stats;

    const float percent = expected == 0 ? 0.0F : static_cast<float>(missed) / static_cast<float>(expected) * PERCENT;
    board.set("Missed", std::format("{:.1f} %", percent),
              levelOf(percent, MISSED_WARNING_PERCENT, MISSED_CRITICAL_PERCENT));
}

}  // namespace ecs::net
