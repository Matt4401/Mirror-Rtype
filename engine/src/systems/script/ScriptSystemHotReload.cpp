/**
 * @file ScriptSystemHotReload.cpp
 * @brief Implementation of live hot-reloading pipeline, staging VM validation, and state replay.
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "systems/script/ScriptSystem.hpp"

extern "C" {
#include <wren.h>
}

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/WrenVM.hpp"

namespace systems {

bool ScriptSystem::validateScriptSource(std::string_view moduleName, std::string_view source) {
    scripting::WrenVM stagingVm;
    stagingVm.setRegistry(_registry);
    stagingVm.setUserContext(this);
    stagingVm.setModuleLoader(_vm.getModuleLoader());
    registerCoreBindings(stagingVm);

    for (auto const& binderFn : _binderReplayList) {
        binderFn(stagingVm);
    }

    auto res = stagingVm.interpret(moduleName, source);
    if (!res) {
        std::cerr << "[ScriptSystem] Hot-reload error in module '" << moduleName << "': " << res.error()
                  << " -> Ignoring modified file to preserve stability.\n";
        return false;
    }
    return true;
}

std::vector<std::pair<std::string, std::string>> ScriptSystem::collectModifiedModules() {
    std::vector<std::pair<std::string, std::string>> modifiedModules;
    std::error_code ec;

    for (auto& [modName, watch] : _watchedScripts) {
        if (!std::filesystem::exists(watch.filePath, ec)) {
            continue;
        }
        auto const currentWriteTime = std::filesystem::last_write_time(watch.filePath, ec);
        if (ec || currentWriteTime == watch.lastWriteTime) {
            continue;
        }

        std::ifstream file(watch.filePath);
        if (!file.is_open()) {
            continue;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string newSource = buffer.str();

        if (!validateScriptSource(modName, newSource)) {
            watch.lastWriteTime = currentWriteTime;
            continue;
        }

        watch.lastGoodSource = newSource;
        modifiedModules.emplace_back(modName, std::move(newSource));
        watch.lastWriteTime = currentWriteTime;
    }
    return modifiedModules;
}

void ScriptSystem::releaseActiveHandles() {
    _entitiesToReinstantiate.clear();
    if (_registry == nullptr || _registry->getComponents<ecs::ScriptComponent>().getDense().empty()) {
        return;
    }
    for (ecs::Entity const e : _registry->getComponents<ecs::ScriptComponent>().getDenseEntities()) {
        auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
        if (script.instanceHandle != nullptr) {
            _entitiesToReinstantiate.push_back(e);
            if (_vm.raw() != nullptr) {
                wrenReleaseHandle(_vm.raw(), static_cast<WrenHandle*>(script.instanceHandle));
            }
            script.instanceHandle = nullptr;
            script.initialized = false;
        }
    }
}

void ScriptSystem::recreateVmAndModules(const std::vector<std::pair<std::string, std::string>>& /*modifiedModules*/) {
    _sleepingFibers.clear();
    _updatingEntities.clear();
    _newHandle.reset();
    _updateHandle.reset();
    _onSpawnHandle.reset();
    _fiberCallHandle.reset();

    std::vector<std::string> cachedSignatures;
    cachedSignatures.reserve(_cachedMethodHandles.size());
    for (auto const& [sig, _] : _cachedMethodHandles) {
        cachedSignatures.push_back(sig);
    }
    _cachedMethodHandles.clear();

    _vm.reset();

    _newHandle = _vm.makeCallHandle("new(_)");
    _updateHandle = _vm.makeCallHandle("update(_)");
    _onSpawnHandle = _vm.makeCallHandle("onSpawn()");
    _fiberCallHandle = _vm.makeCallHandle("call()");

    for (auto const& sig : cachedSignatures) {
        _cachedMethodHandles[sig] = _vm.makeCallHandle(sig);
    }

    auto const dispRes = _vm.interpret("_internal", scripting::INTERNAL_DISPATCHER_SOURCE);
    if (!dispRes) {
        std::cerr << "[ScriptSystem] Failed to interpret _internal: " << dispRes.error() << "\n";
    }

    for (auto const& binderFn : _binderReplayList) {
        binderFn(_vm);
    }

    for (auto const& [modName, watch] : _watchedScripts) {
        if (watch.lastGoodSource.empty()) {
            continue;
        }
        auto const scriptRes = _vm.interpret(modName, watch.lastGoodSource);
        if (!scriptRes) {
            std::cerr << "[ScriptSystem] Failed to reload " << modName << ": " << scriptRes.error() << "\n";
        }
    }
}

void ScriptSystem::reinstantiateActiveScripts() {
    if (_registry == nullptr) {
        _entitiesToReinstantiate.clear();
        return;
    }
    for (ecs::Entity const e : _entitiesToReinstantiate) {
        if (!_registry->getComponents<ecs::ScriptComponent>().contains(e)) {
            continue;
        }
        auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
        instantiateScript(e, script, true);

        if (script.instanceHandle != nullptr && _vm.hasVariable("_internal", "Dispatcher")) {
            ::WrenVM* raw = _vm.raw();
            wrenEnsureSlots(raw, 2);
            _vm.getVariable("_internal", "Dispatcher", 0);
            wrenSetSlotHandle(raw, 1, static_cast<WrenHandle*>(script.instanceHandle));
            auto tryReloadHandle = _vm.makeCallHandle("tryCallReload(_)");
            wrenCall(raw, tryReloadHandle.get());
        }
    }
    _entitiesToReinstantiate.clear();
}

bool ScriptSystem::reloadModifiedScripts() {
    if (_registry == nullptr || _watchedScripts.empty()) {
        return false;
    }

    auto modifiedModules = collectModifiedModules();
    if (modifiedModules.empty()) {
        return false;
    }

    std::cout << "[ScriptSystem] Hot-reloading " << modifiedModules.size() << " script module(s)...\n";
    releaseActiveHandles();
    recreateVmAndModules(modifiedModules);
    reinstantiateActiveScripts();
    std::cout << "[ScriptSystem] Hot-reload complete.\n";
    return true;
}

}  // namespace systems
