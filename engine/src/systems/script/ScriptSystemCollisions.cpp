/**
 * @file ScriptSystemCollisions.cpp
 * @brief Forwarding of physics contact events to the `onCollision(other)` method of entity scripts.
 */

#include <algorithm>
#include <string>

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "events/PhysicsEvents.hpp"
#include "scripting/detail/WrenTypes.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace systems {

namespace {

/** @brief Wren signature of the collision hook; a static string so lookups do not allocate. */
const std::string ON_COLLISION_SIGNATURE = "onCollision(_)";

}  // namespace

void ScriptSystem::invokeEntityMethod(ecs::Entity target, std::string const& signature, ecs::Entity arg) {
    auto itInst = _instanceHandles.find(target);
    if (itInst == _instanceHandles.end()) {
        return;
    }

    auto itMethod = _cachedMethodHandles.find(signature);
    if (itMethod == _cachedMethodHandles.end() || itMethod->second == nullptr) {
        return;
    }

    scripting::detail::UniqueWrenHandle argObj;
    if (_vm.hasVariable("core/Entity", "Entity")) {
        argObj = _vm.newInstanceWithHandle(_newHandle, "core/Entity", "Entity", static_cast<double>(arg));
    }

    if (argObj) {
        _vm.call(itMethod->second, itInst->second.get(), argObj.get());
    } else {
        _vm.call(itMethod->second, itInst->second.get(), static_cast<double>(arg));
    }
}

bool ScriptSystem::isPendingDestroy(ecs::Entity e) const noexcept {
    return std::ranges::find(_pendingDestroyEntities, e) != _pendingDestroyEntities.end();
}

void ScriptSystem::forwardCollision(ecs::Entity target, ecs::Entity other) {
    if (_registry == nullptr || !_registry->hasComponent<ecs::ScriptComponent>(target) ||
        !_registry->getComponents<ecs::ScriptComponent>().get(target).hasCollision) {
        return;
    }
    if (isPendingDestroy(target) || isPendingDestroy(other)) {
        return;
    }
    invokeEntityMethod(target, ON_COLLISION_SIGNATURE, other);
}

void ScriptSystem::forwardCollisions(ecs::EventBus& bus) {
    stopForwardingCollisions();
    if (!_cachedMethodHandles.contains(ON_COLLISION_SIGNATURE)) {
        _cachedMethodHandles[ON_COLLISION_SIGNATURE] = _vm.makeCallHandle(ON_COLLISION_SIGNATURE);
    }
    _collisionBus = &bus;
    _collisionToken = bus.subscribe<events::CollisionBeginEvent>([this](const events::CollisionBeginEvent& event) {
        // Each side is checked right before its call: the first script may destroy either entity.
        forwardCollision(event.entityA, event.entityB);
        forwardCollision(event.entityB, event.entityA);
    });
}

void ScriptSystem::stopForwardingCollisions() noexcept {
    if (_collisionBus != nullptr) {
        _collisionBus->unsubscribe<events::CollisionBeginEvent>(_collisionToken);
        _collisionBus = nullptr;
    }
}

}  // namespace systems
