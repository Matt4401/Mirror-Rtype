#include "gameplay/ShootingPlugin.hpp"

#include <algorithm>
#include <vector>

#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace rtype::server {

namespace {

void spawnBullet(ecs::Registry& registry, const ecs::Entity ship) {
    // Copies: spawning an entity can move the arrays these components are stored in.
    const ecs::Transform transform = registry.getComponents<ecs::Transform>().get(ship);
    const game::EntityNetState shipState = registry.getComponents<game::EntityNetState>().get(ship);

    const ecs::Entity bullet = registry.spawnEntity();
    ::prefabs::buildPlayerBullet(bullet, registry, {{"x", transform.x + SHOT_OFFSET_X}, {"y", transform.y}});
    ecs::net::replicate(registry, bullet, game::KIND_PLAYER_BULLET);
    registry.addComponent(bullet, game::EntityNetState{.subtype = shipState.subtype});
}

void killOffscreenBullets(ecs::Registry& registry) {
    std::vector<ecs::Entity> offscreen;
    registry.runSystem<ecs::Replicated, ecs::Transform>(
        [&offscreen](const ecs::Entity entity, const ecs::Replicated& replicated, const ecs::Transform& transform) {
            if (replicated.kind == game::KIND_PLAYER_BULLET && transform.x > BULLET_MAX_X) {
                offscreen.push_back(entity);
            }
        });
    for (const ecs::Entity entity : offscreen) {
        registry.killEntity(entity);
    }
}

}  // namespace

void ShootingPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::ServerReplicationPlugin>();
    builder.require<systems::PhysicsPlugin>();
}

void ShootingPlugin::init(engine::Engine& engine) {
    _inputs = engine.eventBus().subscribe<ecs::net::InputApplied>([&engine](const ecs::net::InputApplied& event) {
        ecs::Registry& registry = engine.registry();
        if (!registry.hasComponent<ShotCooldown>(event.entity) ||
            !std::ranges::contains(event.input.held, ACTION_SHOOT)) {
            return;
        }
        auto& cooldown = registry.getComponents<ShotCooldown>().get(event.entity);
        if (cooldown.remaining > 0.0F) {
            return;
        }
        cooldown.remaining = SHOT_INTERVAL;
        spawnBullet(registry, event.entity);
    });
}

void ShootingPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    ecs::Registry& registry = engine.registry();
    // Here and not when an input is applied: a step can have no input, and the time passes all the same.
    registry.runSystem<ShotCooldown>([&time](const ecs::Entity /*entity*/, ShotCooldown& cooldown) {
        cooldown.remaining = std::max(0.0F, cooldown.remaining - time.deltaTime);
    });
    killOffscreenBullets(registry);
}

void ShootingPlugin::shutdown(engine::Engine& engine) {
    if (_inputs) {
        engine.eventBus().unsubscribe<ecs::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
}

}  // namespace rtype::server
