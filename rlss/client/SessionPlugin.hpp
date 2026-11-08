/**
 * @file SessionPlugin.hpp
 * @brief Client plugin following the session with the server: closes the game when the connection ends.
 */

#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkClient.hpp"

namespace rlss::client {

/**
 * @brief Closes the game when the connection with the server ends, and logs why.
 *
 * Reacts to the `PeerDisconnected` event published by `ecs::net::NetworkPlugin`.
 */
class SessionPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param transport Client transport, read for the disconnection reason.
     */
    explicit SessionPlugin(std::shared_ptr<ecs::net::INetworkClient> transport);

    /**
     * @brief Gets plugin name.
     * @return String view "SessionPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "SessionPlugin"; }

    /**
     * @brief Declares the dependency on `ecs::net::NetworkPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the disconnection.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the disconnection.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Whether the game stopped because the connection was lost.
     * @return True after a disconnection.
     */
    [[nodiscard]] bool connectionLost() const noexcept { return _lost; }

  private:
    /** @brief Client transport. */
    std::shared_ptr<ecs::net::INetworkClient> _transport;
    /** @brief Set when the server connection ends. */
    bool _lost = false;
    /** @brief Subscription to `PeerDisconnected`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _disconnected;
};

}  // namespace rlss::client
