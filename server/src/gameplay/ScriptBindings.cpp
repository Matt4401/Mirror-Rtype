#include "gameplay/ScriptBindings.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/boss_part/BossPart.hpp"
#include "prefabs/missile/Missile.hpp"
#include "scripting/ComponentBinder.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Binds `ecs::Transform` as `Position` in one VM.
 * @param vm Wren VM.
 * @return True if the generated class was interpreted.
 */
bool bindPosition(scripting::WrenVM& vm) {
    scripting::ComponentBinder<ecs::Transform> binder(vm, POSITION_MODULE, POSITION_CLASS);
    binder.property<&ecs::Transform::x>("x").property<&ecs::Transform::y>("y");
    const auto result = vm.interpret(POSITION_MODULE, binder.generateWrenClass());
    if (!result) {
        log::error(LOG_TAG, "cannot bind ", POSITION_MODULE, " for the scripts: ", result.error());
        return false;
    }
    return true;
}

/** @brief Wren source of the `Combat` class, whose static methods are the foreign functions below. */
constexpr const char* COMBAT_SOURCE = R"(
class Combat {
    foreign static health(id)
    foreign static parts(id)
    foreign static partRole(id)
    foreign static nearestShip(x, y)
    foreign static fireAt(x, y, targetX, targetY, speed)
}
)";

/**
 * @brief Gives the registry of the room a foreign call runs in.
 * @param call Foreign call.
 * @return The registry, or null outside of a room VM.
 */
ecs::Registry* registryOf(const scripting::ForeignCall& call) {
    auto* scripts = call.userData<systems::ScriptSystem>();
    return scripts != nullptr ? scripts->getRegistry() : nullptr;
}

/**
 * @brief `Combat.health(id)`: `[points, max]`, or null for an entity without hit points.
 * @param call Foreign call.
 */
void combatHealth(scripting::ForeignCall& call) {
    const auto id = call.arg<ecs::Entity>(1);
    const ecs::Registry* registry = registryOf(call);
    if (!id || registry == nullptr || !registry->isAlive(*id) || !registry->hasComponent<::prefabs::Health>(*id)) {
        call.retNull();
        return;
    }
    const ::prefabs::Health& health = registry->getComponents<::prefabs::Health>().get(*id);
    call.retList();
    call.listAppend(health.points);
    call.listAppend(health.max);
}

/**
 * @brief `Combat.parts(id)`: ids of the living parts of a boss.
 * @param call Foreign call.
 */
void combatParts(scripting::ForeignCall& call) {
    const auto id = call.arg<ecs::Entity>(1);
    const ecs::Registry* registry = registryOf(call);
    if (!id) {
        return;
    }
    call.retList();
    if (registry == nullptr || !registry->isAlive(*id)) {
        return;
    }
    for (const ecs::Entity child : hierarchy::childrenOf(*registry, *id)) {
        if (registry->isAlive(child) && registry->hasComponent<::prefabs::BossPart>(child)) {
            call.listAppend(static_cast<double>(child));
        }
    }
}

/**
 * @brief `Combat.partRole(id)`: the role name of a boss part, or null.
 * @param call Foreign call.
 */
void combatPartRole(scripting::ForeignCall& call) {
    const auto id = call.arg<ecs::Entity>(1);
    const ecs::Registry* registry = registryOf(call);
    if (!id || registry == nullptr || !registry->isAlive(*id) || !registry->hasComponent<::prefabs::BossPart>(*id)) {
        call.retNull();
        return;
    }
    switch (registry->getComponents<::prefabs::BossPart>().get(*id).role) {
        case ::prefabs::BossPartRole::WeakPoint:
            call.ret(::prefabs::BOSS_PART_WEAK_POINT);
            return;
        case ::prefabs::BossPartRole::Turret:
            call.ret(::prefabs::BOSS_PART_TURRET);
            return;
        case ::prefabs::BossPartRole::Armor:
            call.ret(::prefabs::BOSS_PART_ARMOR);
            return;
    }
}

/**
 * @brief `Combat.nearestShip(x, y)`: `[x, y]` of the nearest player ship that is not respawning, or null.
 * @param call Foreign call.
 */
void combatNearestShip(scripting::ForeignCall& call) {
    const auto x = call.arg<double>(1);
    const auto y = call.arg<double>(2);
    ecs::Registry* registry = registryOf(call);
    if (!x || !y) {
        return;
    }
    if (registry == nullptr) {
        call.retNull();
        return;
    }
    std::optional<std::pair<double, double>> nearest;
    double best = std::numeric_limits<double>::infinity();
    registry->runSystem<ecs::Transform, ecs::Replicated>(
        [&](const ecs::Entity entity, const ecs::Transform& transform, const ecs::Replicated& replicated) {
            if (replicated.kind != game::KIND_PLAYER_SHIP || registry->hasComponent<Respawning>(entity)) {
                return;
            }
            const double dx = transform.x - *x;
            const double dy = transform.y - *y;
            const double distance = (dx * dx) + (dy * dy);
            if (distance < best) {
                best = distance;
                nearest = {transform.x, transform.y};
            }
        });
    if (!nearest) {
        call.retNull();
        return;
    }
    call.retList();
    call.listAppend(nearest->first);
    call.listAppend(nearest->second);
}

/**
 * @brief `Combat.fireAt(x, y, targetX, targetY, speed)`: spawns a missile flying towards the target.
 *
 * Goes through the prefab manager of the scripts, like `Engine.spawnPrefab`, so the missile is replicated and owned by
 * the script. A target on the shooter fires to the left.
 *
 * @param call Foreign call.
 */
void combatFireAt(scripting::ForeignCall& call) {
    const auto x = call.arg<double>(1);
    const auto y = call.arg<double>(2);
    const auto targetX = call.arg<double>(3);
    const auto targetY = call.arg<double>(4);
    const auto speed = call.arg<double>(5);
    auto* scripts = call.userData<systems::ScriptSystem>();
    if (!x || !y || !targetX || !targetY || !speed || scripts == nullptr || scripts->getPrefabManager() == nullptr) {
        return;
    }
    const double clamped = std::clamp(*speed, MIN_AIMED_SHOT_SPEED, MAX_AIMED_SHOT_SPEED);
    double dx = *targetX - *x;
    double dy = *targetY - *y;
    const double length = std::hypot(dx, dy);
    if (length < 1.0) {
        dx = -1.0;
        dy = 0.0;
    } else {
        dx /= length;
        dy /= length;
    }
    const ::prefabs::PrefabParams params{{"x", *x}, {"y", *y}, {"speed", dx * clamped}, {"speed_y", dy * clamped}};
    const systems::ScriptSystem::SpawnDecorator decorator = scripts->spawnDecorator();
    if (decorator == nullptr) {
        (void)scripts->getPrefabManager()->spawn(::prefabs::MISSILE_PREFAB, scripts->getCommandBuffer(), params);
        return;
    }
    (void)scripts->getPrefabManager()->spawn(
        ::prefabs::MISSILE_PREFAB, scripts->getCommandBuffer(), params,
        [decorator, context = scripts->spawnDecoratorContext(), owner = scripts->currentOwner()](
            const ecs::Entity entity, ecs::Registry& registry) { decorator(context, registry, entity, owner); });
}

/**
 * @brief Registers the `Combat` foreign functions and interprets the class in one VM.
 * @param vm Wren VM.
 * @return True if the class was interpreted.
 */
bool bindCombat(scripting::WrenVM& vm) {
    vm.registerForeignMethod<&combatHealth>(COMBAT_MODULE, "Combat", true, "health(_)");
    vm.registerForeignMethod<&combatParts>(COMBAT_MODULE, "Combat", true, "parts(_)");
    vm.registerForeignMethod<&combatPartRole>(COMBAT_MODULE, "Combat", true, "partRole(_)");
    vm.registerForeignMethod<&combatNearestShip>(COMBAT_MODULE, "Combat", true, "nearestShip(_,_)");
    vm.registerForeignMethod<&combatFireAt>(COMBAT_MODULE, "Combat", true, "fireAt(_,_,_,_,_)");
    const auto result = vm.interpret(COMBAT_MODULE, COMBAT_SOURCE);
    if (!result) {
        log::error(LOG_TAG, "cannot bind ", COMBAT_MODULE, " for the scripts: ", result.error());
        return false;
    }
    return true;
}

/**
 * @brief Binds every module of the boss scripts in one VM.
 * @param vm Wren VM.
 * @return True if every module was interpreted.
 */
bool bindAll(scripting::WrenVM& vm) {
    const bool position = bindPosition(vm);
    const bool combat = bindCombat(vm);
    return position && combat;
}

}  // namespace

bool bindScriptComponents(systems::ScriptSystem& scripts) {
    scripts.addBinderReplay([](scripting::WrenVM& vm) { (void)bindAll(vm); });
    return bindAll(scripts.getVm());
}

}  // namespace rtype::server::gameplay
