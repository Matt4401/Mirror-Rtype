#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "metrics/MetricsBoard.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkManager.hpp"

namespace ecs::time {
struct Time;
}

namespace ecs::net {

/**
 * @brief Keeps the network lines of the `metrics::MetricsBoard` up to date.
 *
 * - `Down`, `Up`, `Packets`: traffic of the transport, on any transport that measures it.
 * - `RTT`, `Missed`: from `ReplicationStats`, if the program has a `ClientReplicationPlugin`.
 */
class NetworkMetricsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param transport Transport to read the traffic of.
     */
    explicit NetworkMetricsPlugin(std::shared_ptr<const INetworkManager> transport);

    /**
     * @brief Gets plugin name.
     * @return String view "NetworkMetricsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "NetworkMetricsPlugin"; }

    /**
     * @brief Declares the dependency on `metrics::MetricsPlugin`, which creates the board.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Refreshes the board every `metrics::REFRESH_PERIOD` seconds.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

  private:
    /**
     * @brief Writes the traffic since the last refresh to the board.
     * @param board The board.
     * @param elapsed Seconds since the last refresh.
     */
    void publishTraffic(metrics::MetricsBoard& board, float elapsed);
    /**
     * @brief Writes the round trip and the snapshots missed since the last refresh to the board.
     * @param board The board.
     * @param stats Current replication statistics.
     */
    void publishReplication(metrics::MetricsBoard& board, const ReplicationStats& stats);

    /** @brief Transport to read the traffic of. */
    std::shared_ptr<const INetworkManager> _transport;
    /** @brief Paces the refreshes. */
    metrics::RefreshTimer _timer;
    /** @brief Traffic at the last refresh. */
    TransportStats _lastTraffic;
    /** @brief Replication statistics at the last refresh. */
    ReplicationStats _lastReplication;
};

}  // namespace ecs::net
