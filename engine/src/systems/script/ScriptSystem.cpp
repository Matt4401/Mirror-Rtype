/**
 * @file ScriptSystem.cpp
 * @brief Implementation of ScriptSystem initialization, main loop, and fiber scheduling.
 */

#include "systems/script/ScriptSystem.hpp"

#include <functional>
#include <utility>
#include <vector>

extern "C" {
#include <wren.h>
}

#include "components/ScriptComponent.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace systems {

ScriptSystem::ScriptSystem() = default;

ScriptSystem::~ScriptSystem() { releaseActiveHandles(); }

ScriptSystem::ScriptSystem(ScriptSystem&& other) noexcept
    : _registry(other._registry),
      _bus(other._bus),
      _commandBuffer(std::move(other._commandBuffer)),
      _vm(std::move(other._vm)),
      _updatingEntities(std::move(other._updatingEntities)),
      _pendingDestroyEntities(std::move(other._pendingDestroyEntities)),
      _sleepingFibers(std::move(other._sleepingFibers)),
      _newHandle(std::move(other._newHandle)),
      _updateHandle(std::move(other._updateHandle)),
      _onSpawnHandle(std::move(other._onSpawnHandle)),
      _fiberCallHandle(std::move(other._fiberCallHandle)),
      _cachedMethodHandles(std::move(other._cachedMethodHandles)),
      _watchedScripts(std::move(other._watchedScripts)),
      _entitiesToReinstantiate(std::move(other._entitiesToReinstantiate)),
      _binderReplayList(std::move(other._binderReplayList)),
      _reloadCheckTimer(other._reloadCheckTimer),
      _hotReloadEnabled(other._hotReloadEnabled) {
    _vm.setUserContext(this);
}

ScriptSystem& ScriptSystem::operator=(ScriptSystem&& other) noexcept {
    if (this != &other) {
        releaseActiveHandles();
        _registry = other._registry;
        _bus = other._bus;
        _commandBuffer = std::move(other._commandBuffer);
        _vm = std::move(other._vm);
        _updatingEntities = std::move(other._updatingEntities);
        _pendingDestroyEntities = std::move(other._pendingDestroyEntities);
        _sleepingFibers = std::move(other._sleepingFibers);
        _newHandle = std::move(other._newHandle);
        _updateHandle = std::move(other._updateHandle);
        _onSpawnHandle = std::move(other._onSpawnHandle);
        _fiberCallHandle = std::move(other._fiberCallHandle);
        _cachedMethodHandles = std::move(other._cachedMethodHandles);
        _watchedScripts = std::move(other._watchedScripts);
        _entitiesToReinstantiate = std::move(other._entitiesToReinstantiate);
        _binderReplayList = std::move(other._binderReplayList);
        _reloadCheckTimer = other._reloadCheckTimer;
        _hotReloadEnabled = other._hotReloadEnabled;
        _vm.setUserContext(this);
    }
    return *this;
}

void ScriptSystem::init(ecs::Registry& registry, ecs::EventBus& bus) {
    _registry = &registry;
    _bus = &bus;

    _vm.setRegistry(_registry);
    _vm.setUserContext(this);

    registerCoreBindings(_vm);
    loadCoreModules();

    _newHandle = _vm.makeCallHandle("new(_)");
    _updateHandle = _vm.makeCallHandle("update(_)");
    _onSpawnHandle = _vm.makeCallHandle("onSpawn()");
    _fiberCallHandle = _vm.makeCallHandle("call()");
}

void ScriptSystem::updateSleepingFibers(float dt) {
    std::vector<SleepingFiber> readyFibers;
    for (auto it = _sleepingFibers.begin(); it != _sleepingFibers.end();) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0F) {
            readyFibers.push_back(std::move(*it));
            it = _sleepingFibers.erase(it);
        } else {
            ++it;
        }
    }

    ::WrenVM* raw = _vm.raw();
    for (auto& sleeping : readyFibers) {
        wrenEnsureSlots(raw, 1);
        wrenSetSlotHandle(raw, 0, sleeping.fiber.get());
        wrenCall(raw, _fiberCallHandle.get());
    }
}

void ScriptSystem::updateScriptEntities(float dt) {
    for (ecs::Entity const e : _updatingEntities) {
        if (!_registry->getComponents<ecs::ScriptComponent>().contains(e)) {
            continue;
        }
        auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
        if (script.instanceHandle == nullptr) {
            continue;
        }

        ::WrenVM* raw = _vm.raw();
        wrenEnsureSlots(raw, 2);
        wrenSetSlotHandle(raw, 0, static_cast<WrenHandle*>(script.instanceHandle));
        wrenSetSlotDouble(raw, 1, static_cast<double>(dt));
        wrenCall(raw, _updateHandle.get());
    }

    std::erase_if(_updatingEntities,
                  [this](ecs::Entity e) { return !_registry->getComponents<ecs::ScriptComponent>().contains(e); });
}

void ScriptSystem::flushPendingDestructions() {
    for (ecs::Entity const e : _pendingDestroyEntities) {
        triggerDestroyHook(e);
    }
    _pendingDestroyEntities.clear();
}

void ScriptSystem::update(float dt) {
    if (_registry == nullptr) {
        return;
    }

    if (_hotReloadEnabled) {
        _reloadCheckTimer += dt;
        if (_reloadCheckTimer >= 0.5F) {
            _reloadCheckTimer = 0.0F;
            reloadModifiedScripts();
        }
    }

    if (!_registry->getComponents<ecs::ScriptComponent>().getDense().empty()) {
        for (ecs::Entity const e : _registry->getComponents<ecs::ScriptComponent>().getDenseEntities()) {
            auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
            if (!script.initialized) {
                instantiateScript(e, script);
            }
        }
    }

    updateSleepingFibers(dt);
    updateScriptEntities(dt);
    flushPendingDestructions();
    _commandBuffer.flush(*_registry);
}

void ScriptSystem::enableHotReload(bool enabled) noexcept { _hotReloadEnabled = enabled; }

bool ScriptSystem::isHotReloadEnabled() const noexcept { return _hotReloadEnabled; }

void ScriptSystem::addBinderReplay(std::function<void(scripting::WrenVM&)> callback) {
    _binderReplayList.push_back(std::move(callback));
}

scripting::WrenVM& ScriptSystem::getVm() noexcept { return _vm; }

ecs::CommandBuffer& ScriptSystem::getCommandBuffer() noexcept { return _commandBuffer; }

void ScriptSystem::addSleepingFiber(WrenHandle* fiber, float seconds) {
    _sleepingFibers.push_back(SleepingFiber{
        .fiber = scripting::detail::UniqueWrenHandle(fiber, scripting::detail::WrenHandleDeleter{.vm = _vm.raw()}),
        .timeRemaining = seconds});
}

}  // namespace systems
