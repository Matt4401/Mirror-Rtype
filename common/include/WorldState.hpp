#pragma once
#include <cstdint>
#include <vector>

namespace rtype::common {

// Maps to EntityType in common/proto/ingame.proto
enum class EntityType : std::uint8_t {
    Player        = 0,
    Enemy         = 1,
    PlayerMissile = 2,
    EnemyMissile  = 3,
    Powerup       = 4,
};

// Flags field bit masks (WorldState.EntityState.flags)
enum EntityFlags : std::uint8_t {
    FLAG_ALIVE    = 0x01,
    FLAG_SHOOTING = 0x02,
};

// Maps to EntityState in common/proto/ingame.proto (opcode 0x70 — WORLD_STATE)
struct EntityState {
    std::uint32_t entityId;
    EntityType    type;
    std::uint8_t  subtype; // player index, enemy model, powerup type — see EntityType
    std::int16_t  x;
    std::int16_t  y;
    std::uint8_t  health; // 0 = died this tick (appears exactly once, then gone)
    std::uint8_t  flags;  // see EntityFlags
};

// Maps to WorldState in common/proto/ingame.proto
// tick wraps at UINT32_MAX. Snapshots with tick <= last received MUST be discarded.
struct WorldState {
    std::uint32_t            tick;
    std::vector<EntityState> entities; // max 98 (UDP 1192-byte limit)
};

}  // namespace rtype::common
