#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "INetworkEventSource.hpp"
#include "INetworkService.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace ecs::net {

/**
 * @brief Connects a network transport to the engine loop.
 *
 * Each frame, events taken from the source are published on the EventBus as `PeerConnected`, `PeerDisconnected` and
 * `PacketReceived`. The service is registered in the Mediator so that systems can send. The plugin also owns the
 * `NetworkEntityMap` resource and unbinds an entity from it when the entity is killed.
 */
class NetworkPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param source Where incoming events are read from.
     * @param service What systems use to send; registered in the Mediator on `init`.
     */
    NetworkPlugin(std::shared_ptr<INetworkEventSource> source, std::shared_ptr<engine::INetworkService> service);

    /**
     * @brief Gets plugin name.
     * @return String view "NetworkPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "NetworkPlugin"; }

    /**
     * @brief Declares the dependency on CorePlugin.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers `INetworkService`, creates the `NetworkEntityMap` resource and its destroy listener.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Publishes every event received since the last frame, before the simulation runs.
     * @param engine Host engine.
     */
    void preUpdate(engine::Engine& engine) override;

    /**
     * @brief Removes the destroy listener and unregisters the service.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Incoming events. */
    std::shared_ptr<INetworkEventSource> _source;
    /** @brief Outgoing messages, registered in the Mediator. */
    std::shared_ptr<engine::INetworkService> _service;
    /** @brief Id of the listener unbinding killed entities, while registered. */
    std::optional<std::size_t> _destroyListener;
};

}  // namespace ecs::net
