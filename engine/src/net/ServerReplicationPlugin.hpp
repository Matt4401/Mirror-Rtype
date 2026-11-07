#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "net/IReplicationCodec.hpp"

namespace ecs::time {
struct Time;
}

namespace ecs::net {

/**
 * @brief Server side of the replication: inputs in, snapshots out.
 *
 * - On `PacketReceived`, decodes inputs with the codec and queues them in the `InputBuffer` of the entity whose
 *   `NetworkOwner` is the sender.
 * - On each fixed step, applies one pending input per entity by publishing `InputApplied`.
 * - After the frame's fixed steps (if at least one ran), sends one snapshot of every `NetworkId` + `Replicated`
 *   entity to each peer owning an entity.
 *
 * Requires `NetworkPlugin`, which provides the events and the service.
 */
class ServerReplicationPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param codec Game wire format.
     */
    explicit ServerReplicationPlugin(std::shared_ptr<IReplicationCodec> codec);

    /**
     * @brief Gets plugin name.
     * @return String view "ServerReplicationPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ServerReplicationPlugin"; }

    /**
     * @brief Declares the dependency on NetworkPlugin.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the received packets.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Applies one pending input per entity.
     * @param engine Host engine.
     * @param time Time of this step.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Sends a snapshot if the simulation advanced this frame.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the received packets.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Game wire format. */
    std::shared_ptr<IReplicationCodec> _codec;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Tick of the last fixed step not sent yet. */
    std::optional<std::uint32_t> _pendingTick;
};

}  // namespace ecs::net
