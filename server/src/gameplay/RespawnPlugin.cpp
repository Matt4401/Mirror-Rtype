#include "gameplay/RespawnPlugin.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "prefabs/player_ship/PlayerShip.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Tells whether a ship overlaps a terrain block.
 * @param registry Room registry.
 * @param ship Player ship.
 * @return True if the ship is inside the terrain (touching edges do not count).
 */
bool isInTerrain(ecs::Registry& registry, const ecs::Entity ship) {
    if (!registry.hasComponent<ecs::Transform>(ship)) {
        return false;
    }
    const auto& transform = registry.getComponents<ecs::Transform>().get(ship);
    bool inside = false;
    registry.runSystem<TerrainBlock, ecs::Transform>([&](ecs::Entity /*block*/, const TerrainBlock& block,
                                                         const ecs::Transform& blockTransform) {
        inside = inside ||
                 (std::fabs(transform.x - blockTransform.x) < ::prefabs::PLAYER_SHIP_HALF_WIDTH + block.halfWidth &&
                  std::fabs(transform.y - blockTransform.y) < ::prefabs::PLAYER_SHIP_HALF_HEIGHT + block.halfHeight);
    });
    return inside;
}

}  // namespace

void RespawnPlugin::init(engine::Engine& engine) {
    _bus = &engine.eventBus();
    _destroyedToken = _bus->subscribe<ShipDestroyed>([this, &engine](const ShipDestroyed& destroyed) {
        ecs::Registry& registry = engine.registry();
        if (registry.isAlive(destroyed.ship) && !registry.hasComponent<Respawning>(destroyed.ship)) {
            respawn(registry, destroyed.ship);
        }
    });
}

void RespawnPlugin::respawn(ecs::Registry& registry, const ecs::Entity ship) {
    std::uint8_t playerIndex = 0;
    if (registry.hasComponent<game::EntityNetState>(ship)) {
        game::EntityNetState& state = registry.getComponents<game::EntityNetState>().get(ship);
        playerIndex = state.subtype;
        state.flags |= game::FLAG_INVULNERABLE;
    }
    if (registry.hasComponent<ecs::Transform>(ship)) {
        const ShipSpawnPoint spawn = shipSpawnPoint(playerIndex);
        auto& transform = registry.getComponents<ecs::Transform>().get(ship);
        transform.x = spawn.x;
        transform.y = spawn.y;
    }
    registry.addComponent(ship, Respawning{.secondsLeft = RESPAWN_DURATION});
    _respawning = true;
    _bus->publish(ShipRespawnStarted{.ship = ship});
}

void RespawnPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    ecs::Registry& registry = engine.registry();
    std::vector<ecs::Entity> over;
    bool stillRespawning = false;
    registry.runSystem<Respawning>([&](const ecs::Entity ship, Respawning& respawning) {
        respawning.secondsLeft -= time.deltaTime;
        if (respawning.secondsLeft <= 0.0F) {
            over.push_back(ship);
        } else {
            stillRespawning = true;
        }
    });
    for (const ecs::Entity ship : over) {
        registry.removeComponent<Respawning>(ship);
        if (isInTerrain(registry, ship)) {
            log::info(LOG_TAG, "a ship was still in the terrain at the end of its respawn");
            respawn(registry, ship);
            stillRespawning = true;
            continue;
        }
        if (registry.hasComponent<game::EntityNetState>(ship)) {
            auto& flags = registry.getComponents<game::EntityNetState>().get(ship).flags;
            flags = static_cast<std::uint8_t>(flags & ~game::FLAG_INVULNERABLE);
        }
    }
    // Recounted from the components: a ship removed while respawning (its player left) does not hold the level.
    if (_respawning && !stillRespawning) {
        _respawning = false;
        _bus->publish(AllRespawnsOver{});
    }
}

void RespawnPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_destroyedToken) {
        _bus->unsubscribe<ShipDestroyed>(*_destroyedToken);
        _destroyedToken.reset();
    }
}

}  // namespace rtype::server::gameplay
