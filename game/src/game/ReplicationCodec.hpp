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
#include "protocol/Messages.hpp"

namespace rtype::game {

/** @brief Kind of a player ship (`docs/protocol/packets.md`, appendix A). */
inline constexpr protocol::EntityKind KIND_PLAYER_SHIP = protocol::makeEntityKind("player_ship");
/** @brief Kind of a shot fired by a player; its subtype is the index of the player who fired it. */
inline constexpr protocol::EntityKind KIND_PLAYER_BULLET = protocol::makeEntityKind("player_bullet");
/** @brief Kind of the basic Bydos enemy. */
inline constexpr protocol::EntityKind KIND_ALIEN_SCOUT = protocol::makeEntityKind("alien_scout");
/** @brief Kind of the enemy that fires at the ships. */
inline constexpr protocol::EntityKind KIND_ALIEN_GUNNER = protocol::makeEntityKind("alien_gunner");
/** @brief Kind of the rare, tough enemy; its `health` is the hit points left. */
inline constexpr protocol::EntityKind KIND_ALIEN_ELITE = protocol::makeEntityKind("alien_elite");
/** @brief Kind of the boss ending a level; its `subtype` is its variant, its `health` the hit points left. */
inline constexpr protocol::EntityKind KIND_BOSS_CORE = protocol::makeEntityKind("boss_core");
/** @brief Kind of a piece of a boss (weak point, turret, armor); its `subtype` is its look, its `health` the hit points
 * left of a turret. */
inline constexpr protocol::EntityKind KIND_BOSS_PART = protocol::makeEntityKind("boss_part");
/** @brief Kind of an enemy shot, fired by a boss script. */
inline constexpr protocol::EntityKind KIND_MISSILE = protocol::makeEntityKind("missile");
/** @brief Kind of a power-up, dropped by a boss script and picked up by touching it. */
inline constexpr protocol::EntityKind KIND_POWER_UP = protocol::makeEntityKind("power_up");
/** @brief Width of the default boss hitbox, in pixels; the clients draw a boss without a look at this size. */
inline constexpr float BOSS_CORE_WIDTH = 128.0F;
/** @brief Height of the default boss hitbox, in pixels. */
inline constexpr float BOSS_CORE_HEIGHT = 160.0F;
/** @brief Width of a missile hitbox, in pixels. */
inline constexpr float MISSILE_WIDTH = 16.0F;
/** @brief Height of a missile hitbox, in pixels. */
inline constexpr float MISSILE_HEIGHT = 6.0F;
/** @brief Diameter of a power-up hitbox, in pixels. */
inline constexpr float POWER_UP_SIZE = 24.0F;
/**
 * @brief Kind of a terrain block: a solid block scrolling with the level, `TERRAIN_BLOCK_WIDTH` wide. Its height is
 * `subtype * TERRAIN_HEIGHT_UNIT` pixels, centered on its position.
 */
inline constexpr protocol::EntityKind KIND_OBSTACLE = protocol::makeEntityKind("obstacle");
/**
 * @brief Width of a terrain block, in pixels. Wide blocks keep a section on the screen down to a few replicated
 * entities; a section is as long as a whole number of blocks.
 */
inline constexpr float TERRAIN_BLOCK_WIDTH = 256.0F;
/** @brief Pixels per unit of a terrain column's `subtype`: up to 510 px fit in the byte. */
inline constexpr float TERRAIN_HEIGHT_UNIT = 2.0F;
/** @brief `EntityNetState::flags` bit: the entity is alive. */
inline constexpr std::uint8_t FLAG_ALIVE = 0x01;
/** @brief `EntityNetState::flags` bit: the entity is shooting. */
inline constexpr std::uint8_t FLAG_SHOOTING = 0x02;
/** @brief `EntityNetState::flags` bit: the ship just respawned and cannot be hit; the clients make it blink. */
inline constexpr std::uint8_t FLAG_INVULNERABLE = 0x04;

/**
 * @brief Fields of `protocol::EntityState` the engine does not know, as a component.
 *
 * The server reads it when it writes a snapshot, and the client receives it on each replicated entity. An entity
 * without it is sent with the default values.
 */
struct EntityNetState {
    /** @brief Variant of the kind: player index of a ship or of a player bullet, boss variant, terrain height. */
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
     * @brief Encodes the snapshot, in as many parts as needed (`protocol::MAX_ENTITIES_PER_PART` entities each).
     * @param lastProcessedInput Sequence of the last input of the peer applied by the server.
     * @return The parts; nothing if the snapshot needs more than `protocol::MAX_SNAPSHOT_PARTS` parts.
     */
    [[nodiscard]] std::vector<ecs::net::Bytes> encodeSnapshot(std::uint32_t lastProcessedInput) override;
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
