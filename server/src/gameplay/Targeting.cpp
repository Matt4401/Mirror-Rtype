#include "gameplay/Targeting.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/missile/Missile.hpp"

namespace rtype::server::gameplay {

std::optional<AimPoint> nearestShip(const ecs::Registry& registry, const float x, const float y) {
    std::optional<AimPoint> nearest;
    float best = std::numeric_limits<float>::infinity();
    const auto* replicated = registry.findComponents<ecs::Replicated>();
    if (replicated == nullptr) {
        return nearest;
    }
    for (const ecs::Entity entity : replicated->getDenseEntities()) {
        if (replicated->get(entity).kind != game::KIND_PLAYER_SHIP || registry.hasComponent<Respawning>(entity) ||
            !registry.hasComponent<ecs::Transform>(entity)) {
            continue;
        }
        const ecs::Transform& transform = registry.getComponents<ecs::Transform>().get(entity);
        const float dx = transform.x - x;
        const float dy = transform.y - y;
        const float distance = (dx * dx) + (dy * dy);
        if (distance < best) {
            best = distance;
            nearest = AimPoint{.x = transform.x, .y = transform.y};
        }
    }
    return nearest;
}

Velocity aimAt(const float x, const float y, const AimPoint target, const float speed) noexcept {
    const float dx = target.x - x;
    const float dy = target.y - y;
    const float length = std::hypot(dx, dy);
    if (length < 1.0F) {
        return {.x = -speed, .y = 0.0F};
    }
    return {.x = dx / length * speed, .y = dy / length * speed};
}

void spawnEnemyMissile(ecs::Registry& registry, const float x, const float y, const Velocity velocity,
                       const std::uint8_t look) {
    const ecs::Entity missile = registry.spawnEntity();
    ::prefabs::buildMissile(missile, registry, {{"x", x}, {"y", y}, {"speed", velocity.x}, {"speed_y", velocity.y}});
    registry.addComponent(missile, game::EntityNetState{.subtype = look});
    ecs::net::replicate(registry, missile, game::KIND_MISSILE);
}

}  // namespace rtype::server::gameplay
