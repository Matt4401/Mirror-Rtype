#include "gameplay/ScriptBindings.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "gameplay/Targeting.hpp"
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
    foreign static fireAt(x, y, targetX, targetY, speed, look)
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
    const ecs::Registry* registry = registryOf(call);
    if (!x || !y) {
        return;
    }
    const std::optional<AimPoint> ship =
        registry != nullptr ? nearestShip(*registry, static_cast<float>(*x), static_cast<float>(*y)) : std::nullopt;
    if (!ship) {
        call.retNull();
        return;
    }
    call.retList();
    call.listAppend(ship->x);
    call.listAppend(ship->y);
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
    // `fireAt(x, y, targetX, targetY, speed, look)`: the look of the missile, its subtype, within a byte.
    const std::optional<double> look = call.slotCount() > 6 ? call.arg<double>(6) : std::optional<double>{0.0};
    auto* scripts = call.userData<systems::ScriptSystem>();
    if (!x || !y || !targetX || !targetY || !speed || !look || scripts == nullptr ||
        scripts->getPrefabManager() == nullptr) {
        return;
    }
    const auto clamped = static_cast<float>(std::clamp(*speed, MIN_AIMED_SHOT_SPEED, MAX_AIMED_SHOT_SPEED));
    const Velocity velocity =
        aimAt(static_cast<float>(*x), static_cast<float>(*y),
              AimPoint{.x = static_cast<float>(*targetX), .y = static_cast<float>(*targetY)}, clamped);
    const ::prefabs::PrefabParams params{{"x", *x},
                                         {"y", *y},
                                         {"speed", velocity.x},
                                         {"speed_y", velocity.y},
                                         {"subtype", static_cast<std::int64_t>(std::clamp(*look, 0.0, 255.0))}};
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
    vm.registerForeignMethod<&combatFireAt>(COMBAT_MODULE, "Combat", true, "fireAt(_,_,_,_,_,_)");
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
