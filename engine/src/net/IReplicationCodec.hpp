#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "components/InputBuffer.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/INetworkManager.hpp"
#include "net/SnapshotApplier.hpp"

namespace ecs::net {

/** @brief Largest number of parts one snapshot can be split into. */
inline constexpr std::uint32_t MAX_SNAPSHOT_PARTS = 64;

/** @brief Fields of a snapshot part the engine needs, read before the part is applied. */
struct SnapshotHeader {
    /** @brief Server step of the snapshot; a part older than the last snapshot received is dropped. */
    std::uint32_t tick = 0;
    /** @brief Sequence of the last input of this client applied by the server, for reconciliation. */
    std::uint32_t lastProcessedInput = 0;
    /** @brief Index of this part in the snapshot, below `partCount`. */
    std::uint32_t partIndex = 0;
    /** @brief Number of parts of the snapshot, from 1 to `MAX_SNAPSHOT_PARTS`. */
    std::uint32_t partCount = 1;
};

/**
 * @brief Wire format of the replicated state and of the inputs, provided by the game.
 *
 * The engine decides when to send, to whom, which entities are replicated, and handles the client side bookkeeping
 * (stale snapshots, creation and removal of entities, interpolation, prediction). The codec only translates between
 * the ECS and bytes, so it is the only code that knows the game's protocol.
 *
 * Everything it decodes comes from the network: it must validate sizes and values, and return nothing for a malformed
 * or foreign payload instead of throwing.
 */
class IReplicationCodec {
  public:
    /** @brief Default constructor. */
    IReplicationCodec() = default;
    /** @brief Virtual destructor. */
    virtual ~IReplicationCodec() = default;
    /** @brief Deleted copy constructor: plugins share the codec through `std::shared_ptr`. */
    IReplicationCodec(const IReplicationCodec&) = delete;
    /** @brief Deleted copy assignment operator. */
    IReplicationCodec& operator=(const IReplicationCodec&) = delete;
    /** @brief Deleted move constructor. */
    IReplicationCodec(IReplicationCodec&&) = delete;
    /** @brief Deleted move assignment operator. */
    IReplicationCodec& operator=(IReplicationCodec&&) = delete;

    /**
     * @brief Server: starts a new snapshot, dropping the entities written for the previous one.
     * @param tick Server step of the snapshot.
     */
    virtual void beginSnapshot(std::uint32_t tick) = 0;

    /**
     * @brief Server: adds one entity to the current snapshot.
     * @param registry Server world, to read the entity's components.
     * @param entity Replicated entity.
     * @param id Its network id.
     * @param replicated Its game-defined kind.
     */
    virtual void writeEntity(const ecs::Registry& registry, ecs::Entity entity, std::uint32_t id,
                             const ecs::Replicated& replicated) = 0;

    /**
     * @brief Server: encodes the current snapshot for one peer. Called once per peer, with the same entities.
     *
     * A snapshot that does not fit in one datagram is split: each part holds some of the entities, with the same tick
     * and `lastProcessedInput`, and its `partIndex` / `partCount` in the header `decodeSnapshot()` will read. A world
     * without entities still needs one part: that is how the client learns its entities are gone.
     *
     * @param lastProcessedInput Sequence of the last input of this peer applied by the server.
     * @return The parts, in order, each at most `MAX_UNRELIABLE_PAYLOAD` bytes; a larger or empty one is not sent.
     */
    [[nodiscard]] virtual std::vector<Bytes> encodeSnapshot(std::uint32_t lastProcessedInput) = 0;

    /**
     * @brief Server: reads an input sent by a client.
     * @param data Payload received from the client (untrusted).
     * @return The input, or nothing if the payload is not a valid input.
     */
    [[nodiscard]] virtual std::optional<NetInput> decodeInput(const Bytes& data) = 0;

    /**
     * @brief Client: encodes an input of the local player.
     * @param input Input to send.
     * @return The payload, at most `MAX_UNRELIABLE_PAYLOAD` bytes.
     */
    [[nodiscard]] virtual Bytes encodeInput(const NetInput& input) = 0;

    /**
     * @brief Client: decodes one part of a snapshot and keeps it for `applySnapshot()`.
     * @param data Payload received from the server (untrusted).
     * @return Its header, or nothing if the payload is not a valid snapshot (e.g. another message of the game).
     */
    [[nodiscard]] virtual std::optional<SnapshotHeader> decodeSnapshot(const Bytes& data) = 0;

    /**
     * @brief Client: writes the entities of the last decoded part into the world.
     *
     * Called only if the part is not stale nor already received. Calls `applier.entity()` for each entity of the part,
     * then writes its state (`applier.setTransform()` for its position). Once every part of a snapshot was received,
     * the entities passed to `applier.entity()` by none of them are removed by the engine.
     *
     * @param applier Access to the client world.
     */
    virtual void applySnapshot(SnapshotApplier& applier) = 0;
};

}  // namespace ecs::net
