#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkEvents.hpp"
#include "net/ReplicationEvents.hpp"

namespace ecs::time {
struct Time;
}

namespace ecs::net {

/** @brief Client resource: what the last applied snapshot said. Created by `ClientReplicationPlugin`. */
struct SnapshotState {
    /** @brief Server step of the last snapshot received, even partly; nothing before the first one. */
    std::optional<std::uint32_t> tick;
    /** @brief Sequence of the last local input applied by the server. */
    std::uint32_t lastProcessedInput = 0;
};

/** @brief Client resource: how the replication behaves on this connection. Created by `ClientReplicationPlugin`. */
struct ReplicationStats {
    /**
     * @brief Smoothed time between sending an input and receiving a snapshot saying the server applied it, in ms.
     *
     * It is the responsiveness felt by the player: the network round trip, plus the time the input waits on the server
     * for its step. Nothing before the first acknowledged input.
     */
    std::optional<float> inputRttMs;
    /** @brief Snapshots received in time, each counted once however many parts it has. */
    std::uint64_t snapshotsReceived = 0;
    /**
     * @brief Server steps with no snapshot, seen as a jump in the ticks.
     *
     * Mostly snapshots lost or still on their way, but also the steps a late server caught up in one frame, since it
     * then sends one snapshot for all of them.
     */
    std::uint64_t snapshotsMissed = 0;
    /** @brief Parts of snapshots dropped because a newer snapshot had already arrived. */
    std::uint64_t snapshotsStale = 0;
};

/** @brief Older inputs sent again with each new one, as long as the server has not acknowledged them. */
inline constexpr std::size_t INPUT_RESEND_COUNT = 2;

/**
 * @brief Client side of the replication: snapshots in, inputs out.
 *
 * - On `PacketReceived`, decodes snapshots with the codec, drops the stale ones, and applies the others: entities are
 *   created (`ReplicatedEntitySpawned`) or updated, then the `Predicted` entities are reconciled. A snapshot may come
 *   in several parts, each applied on arrival. Entities are removed when absent (`ReplicatedEntityDespawned`) only
 *   once every part of a snapshot was received: a lost part must not remove the entities it carried.
 * - On `LocalInput` (published by the game), predicts the input on `Predicted` entities and sends it to the server,
 *   with the last `INPUT_RESEND_COUNT` inputs not acknowledged yet: a lost datagram does not cost the server an input,
 *   which would pull the predicted entity back. The server ignores the inputs it already has.
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
     * @brief Creates the `SnapshotState` and `ReplicationStats` resources and subscribes to packets and local inputs.
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
    /** @brief An input already sent, kept to be sent again. */
    struct SentInput {
        /** @brief Sequence of the input. */
        std::uint32_t sequence = 0;
        /** @brief Encoded input. */
        Bytes bytes;
    };

    /** @brief When an input was sent, to measure how long the server takes to acknowledge it. */
    struct PendingInput {
        /** @brief Sequence of the input. */
        std::uint32_t sequence = 0;
        /** @brief Instant of `Clock::now()` when it was sent. */
        double sentAt = 0.0;
    };

    /**
     * @brief Measures the round trip of the latest input acknowledged, if the server acknowledged a new one.
     * @param engine Host engine.
     * @param acknowledged Sequence of the last input applied by the server.
     */
    void measureInputRtt(engine::Engine& engine, std::uint32_t acknowledged);
    /**
     * @brief Applies a packet if it is a part of a snapshot not older than the last one, and not received yet.
     * @param engine Host engine.
     * @param packet Received packet.
     */
    void onPacket(engine::Engine& engine, const PacketReceived& packet);
    /**
     * @brief Predicts a local input and sends it to the server, after the ones not acknowledged yet.
     * @param engine Host engine.
     * @param local The input.
     */
    void onLocalInput(engine::Engine& engine, const LocalInput& local);

    /** @brief Game wire format. */
    std::shared_ptr<IReplicationCodec> _codec;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `LocalInput`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _inputs;
    /** @brief Last inputs sent, oldest first, at most `INPUT_RESEND_COUNT`. */
    std::deque<SentInput> _sentInputs;
    /** @brief Inputs not acknowledged yet, oldest first, at most `MAX_PENDING_INPUTS`. */
    std::deque<PendingInput> _pendingInputs;
    /** @brief Seconds between the previous snapshot and the current one, used to interpolate. */
    float _snapshotInterval = 0.0F;
    /** @brief Parts of the current snapshot received so far, one bit per `partIndex`. */
    std::uint64_t _partsReceived = 0;
    /** @brief Entities contained in the parts of the current snapshot received so far. */
    std::unordered_set<ecs::Entity> _snapshotEntities;
};

}  // namespace ecs::net
