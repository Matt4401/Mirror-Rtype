#include "gameplay/BossFightPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
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
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "log/Logger.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/boss_part/BossPart.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace rtype::server::gameplay {

void BossFightPlugin::build(engine::PluginBuilder& builder) {
    builder.require<systems::PhysicsPlugin>();
    builder.require<hierarchy::HierarchyPlugin>();
}

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
    // The variant goes out as the subtype: the clients pick the look of the boss from it.
    registry.addComponent(boss, game::EntityNetState{
                                    .subtype = registry.getComponents<::prefabs::BossCore>().get(boss).variant,
                                    .health = registry.getComponents<::prefabs::Health>().get(boss).points,
                                });
    if (const auto script = params.find<std::string>("script")) {
        // `scripts/Boss.wren` is the module `scripts/Boss`, read under the script root (the data root, set by the
        // room), whose class is `Boss` unless the event names another one.
        const std::filesystem::path path(*script);
        registry.addComponent(boss, ecs::ScriptComponent{
                                        .moduleName = (path.parent_path() / path.stem()).generic_string(),
                                        .className = params.get<std::string>("class", path.stem().string()),
                                        .hasUpdate = true,
                                    });
    }
    ecs::net::replicate(registry, boss, game::KIND_BOSS_CORE);
    spawnParts(registry, boss, params);
    engine::log::info(LOG_TAG, "boss fight: the scroll stops until the boss is destroyed");
}

void BossFightPlugin::spawnParts(ecs::Registry& registry, const ecs::Entity boss,
                                 const ::prefabs::PrefabParams& params) {
    // A copy: spawning the parts adds transforms, which may move the storage a reference would point into.
    const ecs::Transform center = registry.getComponents<ecs::Transform>().get(boss);
    const auto count = static_cast<std::size_t>(std::clamp<std::int64_t>(
        params.get<std::int64_t>("parts.count", 0), 0, static_cast<std::int64_t>(level::MAX_BOSS_PARTS)));
    for (std::size_t i = 0; i < count; ++i) {
        const std::string prefix = std::format("parts.{}.", i);
        const auto key = [&prefix](const std::string_view field) { return prefix + std::string(field); };
        const auto role = params.get<std::string>(key("role"), "");
        if (!::prefabs::bossPartRole(role).has_value()) {
            engine::log::warning(LOG_TAG, "boss part ", i, ": unknown role '", role, "', skipped");
            continue;
        }
        ::prefabs::PrefabParams part{{"role", role}};
        part.set("x", static_cast<double>(center.x) + params.get(key("x"), 0.0));
        part.set("y", static_cast<double>(center.y) + params.get(key("y"), 0.0));
        for (const std::string_view field : {"width", "height", "health", "look"}) {
            if (const auto value = params.find<double>(key(field))) {
                part.set(field, *value);
            }
        }
        const ecs::Entity entity = registry.spawnEntity();
        ::prefabs::buildBossPart(entity, registry, part);
        const auto& built = registry.getComponents<::prefabs::BossPart>().get(entity);
        const std::uint8_t health = registry.hasComponent<::prefabs::Health>(entity)
                                        ? registry.getComponents<::prefabs::Health>().get(entity).points
                                        : game::EntityNetState{}.health;
        registry.addComponent(entity, game::EntityNetState{.subtype = built.look, .health = health});
        // Placed first, then attached: setParent keeps the world position and computes the offset from it.
        hierarchy::setParent(registry, entity, boss);
        ecs::net::replicate(registry, entity, game::KIND_BOSS_PART);
    }
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
