#include "gameplay/BossFightPlugin.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "LogTag.hpp"
#include "components/RigidBody2D.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "level/Level.hpp"
#include "log/Logger.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace rtype::server::gameplay {

BossFightPlugin::BossFightPlugin(std::shared_ptr<const level::Level> level) : _level(std::move(level)) {}

void BossFightPlugin::build(engine::PluginBuilder& builder) { builder.require<systems::PhysicsPlugin>(); }

void BossFightPlugin::init(engine::Engine& engine) {
    _bus = &engine.eventBus();
    _playedToken = _bus->subscribe<LevelEventPlayed>([this, &engine](const LevelEventPlayed& played) {
        if (played.event.type == level::EVENT_BOSS) {
            startFight(engine.registry(), played.event.params);
        }
    });
}

void BossFightPlugin::startFight(ecs::Registry& registry, const ::prefabs::PrefabParams& params) {
    // The loader only keeps the prefabs accepted by levelPrefab(): a boss event names boss_core.
    if (params.get<std::string>("prefab", "") != ::prefabs::BOSS_CORE_PREFAB) {
        return;
    }
    _fighting = true;
    // Published first, so that the events of the same step already wait for the fight.
    _bus->publish(BossFightStarted{});
    const ecs::Entity boss = registry.spawnEntity();
    ::prefabs::buildBossCore(boss, registry, params);
    registry.addComponent(boss,
                          game::EntityNetState{.health = registry.getComponents<::prefabs::Health>().get(boss).points});
    if (const auto script = params.find<std::string>("script")) {
        // `scripts/Boss.wren` is the module `<data root>/scripts/Boss`, whose class is `Boss` unless the event names
        // another one.
        const std::filesystem::path path = (_level != nullptr ? _level->dataRoot : std::filesystem::path{}) / *script;
        registry.addComponent(boss, ecs::ScriptComponent{
                                        .moduleName = (path.parent_path() / path.stem()).generic_string(),
                                        .className = params.get<std::string>("class", path.stem().string()),
                                        .hasUpdate = true,
                                    });
    }
    ecs::net::replicate(registry, boss, game::KIND_BOSS_CORE);
    engine::log::info(LOG_TAG, "boss fight: the scroll stops until the boss is destroyed");
}

void BossFightPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    bool bossLeft = false;
    engine.registry().runSystem<::prefabs::BossCore, ecs::Transform, ecs::RigidBody2D>(
        [&bossLeft](ecs::Entity /*boss*/, const ::prefabs::BossCore& core, const ecs::Transform& transform,
                    ecs::RigidBody2D& body) {
            bossLeft = true;
            // Flying in: stops once in place. A script may move it afterwards.
            if (body.velocityX < 0.0F && transform.x <= core.stopX) {
                body.velocityX = 0.0F;
            }
        });
    if (_fighting && !bossLeft) {
        _fighting = false;
        engine::log::info(LOG_TAG, "boss destroyed");
        _bus->publish(BossFightEnded{});
    }
}

void BossFightPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_playedToken) {
        _bus->unsubscribe<LevelEventPlayed>(*_playedToken);
        _playedToken.reset();
    }
}

}  // namespace rtype::server::gameplay
