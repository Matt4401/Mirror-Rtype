#include "systems/ScriptSystem.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

extern "C" {
#include <wren.h>
}

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace systems {

namespace {

void engineRegisterWait(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    WrenHandle* fiber = wrenGetSlotHandle(rawVm, 1);
    double const seconds = wrenGetSlotDouble(rawVm, 2);
    scriptSystem->addSleepingFiber(fiber, static_cast<float>(seconds));
}

void engineSpawnPrefab(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    const char* prefabName = wrenGetSlotString(rawVm, 1);
    double const x = wrenGetSlotDouble(rawVm, 2);
    double const y = wrenGetSlotDouble(rawVm, 3);

    std::string const name = (prefabName != nullptr ? prefabName : "");
    scriptSystem->getCommandBuffer().deferSpawn(
        [name, x, y]([[maybe_unused]] ecs::Entity e, [[maybe_unused]] ecs::Registry& reg) {
            // Placeholder prefab spawner
        });
}

void entityDestroy(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    auto const e = static_cast<ecs::Entity>(wrenGetSlotDouble(rawVm, 1));
    scriptSystem->getCommandBuffer().deferDestroy(e);
}

void worldGetEntitiesWithTag(::WrenVM* rawVm) {
    wrenEnsureSlots(rawVm, 1);
    wrenSetSlotNewList(rawVm, 0);
}

void worldGetNearestWithTag(::WrenVM* rawVm) { wrenSetSlotNull(rawVm, 0); }

void worldGetEntitiesInRadius(::WrenVM* rawVm) {
    wrenEnsureSlots(rawVm, 1);
    wrenSetSlotNewList(rawVm, 0);
}

}  // namespace

ScriptSystem::ScriptSystem() = default;

ScriptSystem::~ScriptSystem() = default;

ScriptSystem::ScriptSystem(ScriptSystem&& other) noexcept
    : _registry(other._registry),
      _bus(other._bus),
      _commandBuffer(std::move(other._commandBuffer)),
      _vm(std::move(other._vm)),
      _updatingEntities(std::move(other._updatingEntities)),
      _sleepingFibers(std::move(other._sleepingFibers)),
      _newHandle(std::move(other._newHandle)),
      _updateHandle(std::move(other._updateHandle)),
      _onSpawnHandle(std::move(other._onSpawnHandle)),
      _fiberCallHandle(std::move(other._fiberCallHandle)),
      _cachedMethodHandles(std::move(other._cachedMethodHandles)) {
    _vm.setUserContext(this);
}

ScriptSystem& ScriptSystem::operator=(ScriptSystem&& other) noexcept {
    if (this != &other) {
        _registry = other._registry;
        _bus = other._bus;
        _commandBuffer = std::move(other._commandBuffer);
        _vm = std::move(other._vm);
        _updatingEntities = std::move(other._updatingEntities);
        _sleepingFibers = std::move(other._sleepingFibers);
        _newHandle = std::move(other._newHandle);
        _updateHandle = std::move(other._updateHandle);
        _onSpawnHandle = std::move(other._onSpawnHandle);
        _fiberCallHandle = std::move(other._fiberCallHandle);
        _cachedMethodHandles = std::move(other._cachedMethodHandles);
        _vm.setUserContext(this);
    }
    return *this;
}

void ScriptSystem::init(ecs::Registry& registry, ecs::EventBus& bus) {
    _registry = &registry;
    _bus = &bus;

    _vm.setRegistry(_registry);
    _vm.setUserContext(this);

    registerCoreBindings();
    loadCoreModules();

    _newHandle = _vm.makeCallHandle("new(_)");
    _updateHandle = _vm.makeCallHandle("update(_)");
    _onSpawnHandle = _vm.makeCallHandle("onSpawn()");
    _fiberCallHandle = _vm.makeCallHandle("call()");
}

void ScriptSystem::registerCoreBindings() {
    _vm.registerForeignMethod("core/Engine", "Engine", true, "registerWait(_,_)", &engineRegisterWait);
    _vm.registerForeignMethod("core/Engine", "Engine", true, "spawnPrefab(_,_,_)", &engineSpawnPrefab);
    _vm.registerForeignMethod("core/Entity", "Entity", true, "destroyEntity(_)", &entityDestroy);
    _vm.registerForeignMethod("core/World", "World", true, "getEntitiesWithTag(_)", &worldGetEntitiesWithTag);
    _vm.registerForeignMethod("core/World", "World", true, "getNearestWithTag(_,_,_)", &worldGetNearestWithTag);
    _vm.registerForeignMethod("core/World", "World", true, "getEntitiesInRadius(_,_,_)", &worldGetEntitiesInRadius);
}

void ScriptSystem::loadCoreModules() {
    _vm.setModuleLoader([](std::string_view moduleName) -> std::string {
        std::string_view const coreSource = scripting::getCoreModuleSource(moduleName);
        if (!coreSource.empty()) {
            return std::string(coreSource);
        }

        std::string const filename = std::string(moduleName) + ".wren";
        std::ifstream file(filename);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        return "";
    });

    auto const res = _vm.interpret("_internal", scripting::INTERNAL_DISPATCHER_SOURCE);
    (void)res;
}

void ScriptSystem::instantiateScript(ecs::Entity e, ecs::ScriptComponent& script) {
    if (script.initialized) {
        return;
    }

    if (!_vm.hasModule(script.moduleName)) {
        std::string filename = std::string(script.moduleName);
        if (!filename.ends_with(".wren")) {
            filename += ".wren";
        }
        std::ifstream file(filename);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            auto res = _vm.interpret(script.moduleName, buffer.str());
            if (!res) {
                std::cerr << "[ScriptSystem] Failed to load module " << script.moduleName << ": " << res.error()
                          << "\n";
                return;
            }
        } else {
            std::cerr << "[ScriptSystem] Module file not found: " << filename << "\n";
            return;
        }
    }

    ::WrenVM* raw = _vm.raw();
    if (!_vm.hasVariable(script.moduleName, script.className)) {
        std::cerr << "[ScriptSystem] Class " << script.className << " not found in " << script.moduleName << "\n";
        return;
    }

    wrenEnsureSlots(raw, 2);
    WrenHandle* entityArgHandle = nullptr;
    if (_vm.hasVariable("core/Entity", "Entity")) {
        _vm.getVariable("core/Entity", "Entity", 0);
        wrenSetSlotDouble(raw, 1, static_cast<double>(e));
        if (wrenCall(raw, _newHandle.get()) == WREN_RESULT_SUCCESS) {
            entityArgHandle = wrenGetSlotHandle(raw, 0);
        }
    }

    wrenEnsureSlots(raw, 2);
    _vm.getVariable(script.moduleName, script.className, 0);
    if (entityArgHandle != nullptr) {
        wrenSetSlotHandle(raw, 1, entityArgHandle);
        wrenReleaseHandle(raw, entityArgHandle);
    } else {
        wrenSetSlotDouble(raw, 1, static_cast<double>(e));
    }

    if (wrenCall(raw, _newHandle.get()) != WREN_RESULT_SUCCESS) {
        return;
    }

    script.instanceHandle = wrenGetSlotHandle(raw, 0);
    script.initialized = true;

    if (_vm.hasVariable("_internal", "Dispatcher")) {
        wrenEnsureSlots(raw, 2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        wrenSetSlotHandle(raw, 1, static_cast<WrenHandle*>(script.instanceHandle));
        auto trySpawnHandle = _vm.makeCallHandle("tryCallSpawn(_)");
        wrenCall(raw, trySpawnHandle.get());
    }

    if (script.hasUpdate) {
        _updatingEntities.push_back(e);
    }
}

void ScriptSystem::update(float dt) {
    if (_registry == nullptr) {
        return;
    }

    if (!_registry->getComponents<ecs::ScriptComponent>().getDense().empty()) {
        for (ecs::Entity const e : _registry->getComponents<ecs::ScriptComponent>().getDenseEntities()) {
            auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
            if (!script.initialized) {
                instantiateScript(e, script);
            }
        }
    }

    for (auto it = _sleepingFibers.begin(); it != _sleepingFibers.end();) {
        it->timeRemaining -= dt;
        if (it->timeRemaining <= 0.0F) {
            ::WrenVM* raw = _vm.raw();
            wrenEnsureSlots(raw, 1);
            wrenSetSlotHandle(raw, 0, it->fiber.get());
            wrenCall(raw, _fiberCallHandle.get());
            it = _sleepingFibers.erase(it);
        } else {
            ++it;
        }
    }

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

    _commandBuffer.flush(*_registry);
}

scripting::WrenVM& ScriptSystem::getVm() noexcept { return _vm; }

ecs::CommandBuffer& ScriptSystem::getCommandBuffer() noexcept { return _commandBuffer; }

void ScriptSystem::addSleepingFiber(WrenHandle* fiber, float seconds) {
    _sleepingFibers.push_back(SleepingFiber{
        .fiber = scripting::detail::UniqueWrenHandle(fiber, scripting::detail::WrenHandleDeleter{.vm = _vm.raw()}),
        .timeRemaining = seconds});
}

}  // namespace systems
