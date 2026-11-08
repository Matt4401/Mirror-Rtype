/**
 * @file ScriptSystem.cpp
 * @brief Implementation of ScriptSystem initialization, main loop, and fiber scheduling.
 */

#include "systems/script/ScriptSystem.hpp"

#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "components/ScriptComponent.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "prefabs/PrefabManager.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace systems {

ScriptSystem::ScriptSystem() = default;

ScriptSystem::~ScriptSystem() {
    detachDestroyListener();
    stopForwardingCollisions();
    releaseActiveHandles();
}

ScriptSystem::ScriptSystem(ScriptSystem&& other) noexcept
    : _registry(other._registry),
      _bus(other._bus),
      _prefabs(other._prefabs),
      _commandBuffer(std::move(other._commandBuffer)),
      _vm(std::move(other._vm)),
      _updatingEntities(std::move(other._updatingEntities)),
      _pendingDestroyEntities(std::move(other._pendingDestroyEntities)),
      _queryBuffer(std::move(other._queryBuffer)),
      _sleepingFibers(std::move(other._sleepingFibers)),
      _instanceHandles(std::move(other._instanceHandles)),
      _newHandle(std::move(other._newHandle)),
      _updateHandle(std::move(other._updateHandle)),
      _onSpawnHandle(std::move(other._onSpawnHandle)),
      _fiberCallHandle(std::move(other._fiberCallHandle)),
      _collisionBus(std::exchange(other._collisionBus, nullptr)),
      _collisionToken(other._collisionToken),
      _collisionTarget(std::move(other._collisionTarget)),
      _cachedMethodHandles(std::move(other._cachedMethodHandles)),
      _watchedScripts(std::move(other._watchedScripts)),
      _entitiesToReinstantiate(std::move(other._entitiesToReinstantiate)),
      _binderReplayList(std::move(other._binderReplayList)),
      _reloadCheckTimer(other._reloadCheckTimer),
      _hotReloadEnabled(other._hotReloadEnabled),
      _lastReplaySucceeded(other._lastReplaySucceeded),
      _extensions(std::move(other._extensions)),
      _extraModules(std::move(other._extraModules)) {
    other.detachDestroyListener();
    _vm.setUserContext(this);
    attachDestroyListener();
    if (_collisionTarget) {
        *_collisionTarget = this;
    }
}

ScriptSystem& ScriptSystem::operator=(ScriptSystem&& other) noexcept {
    if (this != &other) {
        detachDestroyListener();
        other.detachDestroyListener();
        releaseActiveHandles();
        _sleepingFibers.clear();
        _newHandle.reset();
        _updateHandle.reset();
        _onSpawnHandle.reset();
        _fiberCallHandle.reset();
        _cachedMethodHandles.clear();
        _registry = other._registry;
        _bus = other._bus;
        _prefabs = other._prefabs;
        _commandBuffer = std::move(other._commandBuffer);
        _vm = std::move(other._vm);
        _updatingEntities = std::move(other._updatingEntities);
        _pendingDestroyEntities = std::move(other._pendingDestroyEntities);
        _queryBuffer = std::move(other._queryBuffer);
        _sleepingFibers = std::move(other._sleepingFibers);
        _instanceHandles = std::move(other._instanceHandles);
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
        _lastReplaySucceeded = other._lastReplaySucceeded;
        _extensions = std::move(other._extensions);
        _extraModules = std::move(other._extraModules);
        _vm.setUserContext(this);
        attachDestroyListener();
        std::swap(_collisionBus, other._collisionBus);
        std::swap(_collisionToken, other._collisionToken);
        std::swap(_collisionTarget, other._collisionTarget);
        if (_collisionTarget) {
            *_collisionTarget = this;
        }
        if (other._collisionTarget) {
            *other._collisionTarget = &other;
        }
    }
    return *this;
}

void ScriptSystem::attachDestroyListener() {
    if (_registry == nullptr || _destroyListenerId.has_value()) {
        return;
    }
    _destroyListenerId = _registry->addDestroyListener([this](ecs::Entity e) { triggerDestroyHook(e); });
}

void ScriptSystem::detachDestroyListener() noexcept {
    if (_registry != nullptr && _destroyListenerId.has_value()) {
        _registry->removeDestroyListener(*_destroyListenerId);
    }
    _destroyListenerId.reset();
}

void ScriptSystem::init(ecs::Registry& registry, ecs::EventBus& bus) {
    detachDestroyListener();
    _registry = &registry;
    attachDestroyListener();
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
    std::vector<SleepingFiber>& readyFibers = _extensions.readyFibers;
    for (auto it = _sleepingFibers.begin(); it != _sleepingFibers.end();) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0F) {
            readyFibers.push_back(std::move(*it));
            it = _sleepingFibers.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& sleeping : readyFibers) {
        const OwnerScope scope(*this, sleeping.owner);
        auto const res = _vm.call(_fiberCallHandle, sleeping.fiber.get());
        if (res != WREN_RESULT_SUCCESS) {
            std::cerr << "[ScriptSystem] Warning: Resuming sleeping fiber failed\n";
        }
    }
    readyFibers.clear();
}

void ScriptSystem::updateScriptEntities(float dt) {
    for (ecs::Entity const e : _updatingEntities) {
        auto it = _instanceHandles.find(e);
        if (it == _instanceHandles.end()) {
            continue;
        }

        const OwnerScope scope(*this, e);
        auto const res = _vm.call(_updateHandle, it->second.get(), static_cast<double>(dt));
        if (res != WREN_RESULT_SUCCESS) {
            std::cerr << "[ScriptSystem] Warning: Script update loop failed for entity " << e << "\n";
        }
    }

    std::erase_if(_updatingEntities, [this](ecs::Entity e) { return !_instanceHandles.contains(e); });
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

void ScriptSystem::setPrefabManager(prefabs::PrefabManager const* manager) noexcept { _prefabs = manager; }

prefabs::PrefabManager const* ScriptSystem::getPrefabManager() const noexcept { return _prefabs; }

std::vector<ecs::Entity>& ScriptSystem::getQueryBuffer() noexcept { return _queryBuffer; }

void ScriptSystem::addSleepingFiber(scripting::detail::UniqueWrenHandle fiber, float seconds) {
    _sleepingFibers.push_back(
        SleepingFiber{.fiber = std::move(fiber), .timeRemaining = seconds, .owner = _extensions.currentOwner});
}

void ScriptSystem::cancelFibers(ScriptOwner owner) {
    std::erase_if(_sleepingFibers, [owner](const SleepingFiber& sleeping) { return sleeping.owner == owner; });
}

void ScriptSystem::addModuleSource(std::string name, std::string source) {
    (*_extraModules)[std::move(name)] = std::move(source);
}

std::size_t ScriptSystem::addVmResetListener(std::function<void(VmResetPhase)> listener) {
    const std::size_t id = _extensions.nextListenerId++;
    _extensions.vmResetListeners.emplace_back(id, std::move(listener));
    return id;
}

void ScriptSystem::removeVmResetListener(std::size_t id) {
    std::erase_if(_extensions.vmResetListeners, [id](const auto& entry) { return entry.first == id; });
}

void ScriptSystem::notifyVmReset(VmResetPhase phase) {
    for (auto& [id, listener] : _extensions.vmResetListeners) {
        if (listener) {
            listener(phase);
        }
    }
}

}  // namespace systems
