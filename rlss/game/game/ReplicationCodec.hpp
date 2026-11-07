/**
 * @file ReplicationCodec.hpp
 * @brief RLSS wire format of the replicated world and of the player inputs, plugged into the engine replication.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "components/InputBuffer.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/INetworkManager.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/SnapshotApplier.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::game {

/**
 * @brief Translates between the ECS and the RLSS messages for `ecs::net::ServerReplicationPlugin` and
 * `ecs::net::ClientReplicationPlugin`.
 *
 * A replicated entity carries its `ecs::Transform` (facing = sign of `scaleX`); a car also its `CarLook` and
 * `CarStatus`, the ball its `ShotGlow`. On the client, a new
 * entity also gets `ecs::Interpolated`, so that it moves smoothly between snapshots.
 */
class ReplicationCodec final : public ecs::net::IReplicationCodec {
  public:
    /**
     * @brief Creates the codec of one match.
     * @param matchId Server: match written in every snapshot. Ignored by a client, which filters snapshots before.
     */
    explicit ReplicationCodec(std::uint16_t matchId = 0) : _matchId(matchId) {}

    /**
     * @brief Server: starts a new snapshot.
     * @param tick Server step.
     */
    void beginSnapshot(std::uint32_t tick) override;

    /**
     * @brief Server: adds one entity to the current snapshot (skipped if it has no `ecs::Transform`).
     * @param registry Server world.
     * @param entity Replicated entity.
     * @param id Its network id.
     * @param replicated Its kind.
     */
    void writeEntity(const ecs::Registry& registry, ecs::Entity entity, std::uint32_t id,
                     const ecs::Replicated& replicated) override;

    /**
     * @brief Server: encodes the current snapshot, split into parts of at most `protocol::MAX_ENTITIES_PER_PART`.
     * @param lastProcessedInput Sequence of the last input of the receiving peer applied by the server.
     * @return The parts (at least one).
     */
    [[nodiscard]] std::vector<ecs::net::Bytes> encodeSnapshot(std::uint32_t lastProcessedInput) override;

    /**
     * @brief Server: reads a `PlayerInput` message.
     * @param data Untrusted payload.
     * @return The input (`JUMP_ID` / `BOOST_ID` held, `STEER_ID` axis), or nothing for any other payload.
     */
    [[nodiscard]] std::optional<ecs::NetInput> decodeInput(const ecs::net::Bytes& data) override;

    /**
     * @brief Client: encodes the local input as a `PlayerInput` message.
     * @param input Input of this step.
     * @return Wire bytes.
     */
    [[nodiscard]] ecs::net::Bytes encodeInput(const ecs::NetInput& input) override;

    /**
     * @brief Client: decodes a snapshot part and keeps it for `applySnapshot()`.
     * @param data Untrusted payload.
     * @return Its header, or nothing for any other payload.
     */
    [[nodiscard]] std::optional<ecs::net::SnapshotHeader> decodeSnapshot(const ecs::net::Bytes& data) override;

    /**
     * @brief Client: writes the entities of the last decoded part into the world.
     * @param applier Access to the client world.
     */
    void applySnapshot(ecs::net::SnapshotApplier& applier) override;

  private:
    /** @brief Server: match written in the snapshots. */
    std::uint16_t _matchId;
    /** @brief Server: tick of the snapshot being written. */
    std::uint32_t _tick = 0;
    /** @brief Server: entities of the snapshot being written. */
    std::vector<protocol::EntityState> _entities;
    /** @brief Client: entities of the last decoded part. */
    std::vector<protocol::EntityState> _received;
};

}  // namespace rlss::game
