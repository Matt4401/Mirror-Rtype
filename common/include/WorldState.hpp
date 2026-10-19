#pragma once
#include <cstdint>
#include <vector>

namespace rtype::common {

enum class EntityType : std::uint8_t {
    Player = 0,
    Enemy = 1,
    PlayerMissile = 2,
    EnemyMissile = 3,
    Powerup = 4,
};

enum class EntityFlags : std::uint8_t {
    Alive = 0x01,
    Shooting = 0x02,
};

struct EntityState {
    std::uint32_t entityId;
    EntityType type;
    std::uint8_t subtype;
    std::int16_t x;
    std::int16_t y;
    std::uint8_t health;
    std::uint8_t flags;
};

struct WorldState {
    std::vector<EntityState> entities;
};

}  // namespace rtype::common
