#include "gameplay/ScriptPrefabs.hpp"

#include <array>
#include <string_view>
#include <utility>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/power_up/PowerUp.hpp"
#include "protocol/Messages.hpp"

namespace rtype::server::gameplay {

namespace {

/** @brief Builder of an R-Type prefab, as declared by each `prefabs/<name>/` header. */
using Builder = void (*)(ecs::Entity, ecs::Registry&, const ::prefabs::PrefabParams&);

/** @brief Prefabs a script may spawn, with their builder. */
constexpr std::array<std::pair<std::string_view, Builder>, 5> SCRIPT_PREFABS{{
    {::prefabs::ALIEN_SCOUT_PREFAB, &::prefabs::buildAlienScout},
    {::prefabs::MISSILE_PREFAB, &::prefabs::buildMissile},
    {::prefabs::POWER_UP_PREFAB, &::prefabs::buildPowerUp},
    {::prefabs::OBSTACLE_PREFAB, &::prefabs::buildObstacle},
    {::prefabs::BOSS_CORE_PREFAB, &::prefabs::buildBossCore},
}};

}  // namespace

void registerScriptPrefabs(::prefabs::PrefabManager& manager) {
    for (const auto& [name, build] : SCRIPT_PREFABS) {
        const protocol::EntityKind kind = protocol::makeEntityKind(name);
        manager.registerPrefab(name, [build, kind](const ecs::Entity entity, ecs::Registry& registry,
                                                   const ::prefabs::PrefabParams& params) {
            build(entity, registry, params);
            registry.addComponent(entity, game::EntityNetState{});
            ecs::net::replicate(registry, entity, kind);
        });
    }
}

}  // namespace rtype::server::gameplay
