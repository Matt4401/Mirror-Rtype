#pragma once

#include <cstdint>
#include <optional>

#include "components/InputBuffer.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/INetworkManager.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/SnapshotApplier.hpp"
#include "protocol/Messages.hpp"

namespace rtype::game {

/** @brief Kind of a player ship (`docs/protocol/packets.md`, appendix A). */
inline constexpr protocol::EntityKind KIND_PLAYER_SHIP = protocol::makeEntityKind("player_ship");
/** @brief `EntityNetState::flags` bit: the entity is alive. */
inline constexpr std::uint8_t FLAG_ALIVE = 0x01;
/** @brief `EntityNetState::flags` bit: the entity is shooting. */
inline constexpr std::uint8_t FLAG_SHOOTING = 0x02;

/**
 * @brief Fields of `protocol::EntityState` the engine does not know, as a component.
 *
 * The server reads it when it writes a snapshot, and the client receives it on each replicated entity. An entity
 * without it is sent with the default values.
 */
struct EntityNetState {
    /** @brief Variant of the kind: player index of a ship or of a player bullet, 0 otherwise. */
    std::uint8_t subtype = 0;
    /** @brief Health points; 0 means the entity died this tick. */
    std::uint8_t health = 1;
    /** @brief `FLAG_*` bits. */
    std::uint8_t flags = FLAG_ALIVE;
};

/**
 * @brief Replication wire format of R-Type: `protocol::WorldState` and `protocol::PlayerInput`.
 *
 * Keeps the snapshot being written and the last one decoded, so an instance must not be shared between engines.
 */
class ReplicationCodec final : public ecs::net::IReplicationCodec {
  public:
    /**
     * @brief Starts a snapshot.
     * @param tick Server step of the snapshot.
     */
    void beginSnapshot(std::uint32_t tick) override;
    /**
     * @brief Adds an entity to the snapshot. Skipped if its position is not finite: `protocol::decode()` would reject
     * the whole snapshot.
     * @param registry Server world.
     * @param entity Replicated entity.
     * @param id Its network id.
     * @param replicated Its kind.
     */
    void writeEntity(const ecs::Registry& registry, ecs::Entity entity, std::uint32_t id,
                     const ecs::Replicated& replicated) override;
    /**
     * @brief Encodes the snapshot for one peer.
     * @param lastProcessedInput Sequence of the last input of this peer applied by the server.
     * @return The payload; empty, after logging an error, if it does not fit in a datagram.
     */
    [[nodiscard]] ecs::net::Bytes encodeSnapshot(std::uint32_t lastProcessedInput) override;
    /**
     * @brief Reads an input.
     * @param data Untrusted payload.
     * @return The input, or nothing if the payload is not a valid `protocol::PlayerInput`.
     */
    [[nodiscard]] std::optional<ecs::NetInput> decodeInput(const ecs::net::Bytes& data) override;
    /**
     * @brief Encodes an input.
     * @param input Input to send.
     * @return The payload.
     */
    [[nodiscard]] ecs::net::Bytes encodeInput(const ecs::NetInput& input) override;
    /**
     * @brief Decodes a snapshot and keeps it for `applySnapshot()`.
     * @param data Untrusted payload.
     * @return Its header, or nothing if the payload is not a valid `protocol::WorldState`.
     */
    [[nodiscard]] std::optional<ecs::net::SnapshotHeader> decodeSnapshot(const ecs::net::Bytes& data) override;
    /**
     * @brief Writes the entities of the last decoded snapshot into the client world.
     *
     * Only the position is on the wire: the other fields of the `Transform` (scale, rotation, depth) stay as the
     * client set them.
     *
     * @param applier Access to the client world.
     */
    void applySnapshot(ecs::net::SnapshotApplier& applier) override;

  private:
    /** @brief Snapshot being written (server). */
    protocol::WorldState _outgoing;
    /** @brief Last decoded snapshot (client). */
    protocol::WorldState _incoming;
};

}  // namespace rtype::game
