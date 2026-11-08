#include "gameplay/ScriptPrefabs.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/power_up/PowerUp.hpp"
#include "protocol/Messages.hpp"

namespace rtype::server::gameplay {

namespace {

/** @brief Builder of an R-Type prefab, as declared by each `prefabs/<name>/` header. */
using Builder = void (*)(ecs::Entity, ecs::Registry&, const ::prefabs::PrefabParams&);

/**
 * @brief Prefabs a script may spawn, with their builder.
 *
 * `boss_core` is left out: a boss stops the scroll until it is destroyed, which only a level `boss` event sets up.
 */
constexpr std::array<std::pair<std::string_view, Builder>, 4> SCRIPT_PREFABS{{
    {::prefabs::ALIEN_SCOUT_PREFAB, &::prefabs::buildAlienScout},
    {::prefabs::MISSILE_PREFAB, &::prefabs::buildMissile},
    {::prefabs::POWER_UP_PREFAB, &::prefabs::buildPowerUp},
    {::prefabs::OBSTACLE_PREFAB, &::prefabs::buildObstacle},
}};

}  // namespace

void registerScriptPrefabs(::prefabs::PrefabManager& manager) {
    for (const auto& [name, build] : SCRIPT_PREFABS) {
        const protocol::EntityKind kind = protocol::makeEntityKind(name);
        manager.registerPrefab(name, [build, kind](const ecs::Entity entity, ecs::Registry& registry,
                                                   const ::prefabs::PrefabParams& params) {
            build(entity, registry, params);
            // A prefab that survives several hits replicates its hit points, like the level bosses.
            const std::uint8_t health = registry.hasComponent<::prefabs::Health>(entity)
                                            ? registry.getComponents<::prefabs::Health>().get(entity).points
                                            : game::EntityNetState{}.health;
            registry.addComponent(entity, game::EntityNetState{.health = health});
            ecs::net::replicate(registry, entity, kind);
        });
    }
}

}  // namespace rtype::server::gameplay
