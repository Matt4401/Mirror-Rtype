#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkEvents.hpp"

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

/** @brief Client resource: what the last applied snapshot said. Created by `ClientReplicationPlugin`. */
struct SnapshotState {
    /** @brief Server step of the last applied snapshot, or nothing before the first one. */
    std::optional<std::uint32_t> tick;
    /** @brief Sequence of the last local input applied by the server. */
    std::uint32_t lastProcessedInput = 0;
};

/**
 * @brief Client side of the replication: snapshots in, inputs out.
 *
 * - On `PacketReceived`, decodes snapshots with the codec, drops the stale ones, and applies the others: entities are
 *   created (`ReplicatedEntitySpawned`), updated, or removed when absent (`ReplicatedEntityDespawned`), then the
 *   `Predicted` entities are reconciled.
 * - On `LocalInput` (published by the game), sends the input to the server and predicts it on `Predicted` entities.
 * - Each frame, moves the `Interpolated` entities toward their last snapshot.
 *
 * The server tick is assumed to last `Engine::fixedDeltaTime()` seconds, like the client's.
 * Requires `NetworkPlugin`, which provides the events, the service and the `NetworkEntityMap`.
 */
class ClientReplicationPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param codec Game wire format.
     */
    explicit ClientReplicationPlugin(std::shared_ptr<IReplicationCodec> codec);

    /**
     * @brief Gets plugin name.
     * @return String view "ClientReplicationPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ClientReplicationPlugin"; }

    /**
     * @brief Declares the dependency on NetworkPlugin.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Creates the `SnapshotState` resource and subscribes to packets and local inputs.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Interpolates the `Interpolated` entities.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Applies a packet if it is a snapshot newer than the last one.
     * @param engine Host engine.
     * @param packet Received packet.
     */
    void onPacket(engine::Engine& engine, const PacketReceived& packet);

    /** @brief Game wire format. */
    std::shared_ptr<IReplicationCodec> _codec;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `LocalInput`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _inputs;
};

}  // namespace ecs::net
