/**
 * @file ScriptSystemHotReload.cpp
 * @brief Implementation of live hot-reloading pipeline, staging VM validation, and state replay.
 */

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "log/Logger.hpp"
#include "assets/AssetPath.hpp"
#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "systems/script/ScriptSystem.hpp"
#include "systems/script/detail/ScriptSupport.hpp"

namespace systems {

using scripting::detail::LOG_TAG;

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
        engine::log::warning(LOG_TAG, "hot reload: script module ", assets::quoteForLog(moduleName),
                            " ignored, the previous version keeps running: ", detail::formatScriptError(res.error()));
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
    for (auto const& [e, _] : _instanceHandles) {
        _entitiesToReinstantiate.push_back(e);
        if (_registry != nullptr && _registry->getComponents<ecs::ScriptComponent>().contains(e)) {
            _registry->getComponents<ecs::ScriptComponent>().get(e).initialized = false;
        }
    }
    _instanceHandles.clear();
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
        engine::log::error(
            LOG_TAG, "hot reload: cannot run the internal dispatcher: ", detail::formatScriptError(dispRes.error()));
    }

    for (auto const& binderFn : _binderReplayList) {
        binderFn(_vm);
    }

    _lastReplaySucceeded = true;
    for (auto const& [modName, watch] : _watchedScripts) {
        if (watch.lastGoodSource.empty()) {
            continue;
        }
        auto const scriptRes = _vm.interpret(modName, watch.lastGoodSource);
        if (!scriptRes) {
            _lastReplaySucceeded = false;
            engine::log::warning(LOG_TAG, "hot reload: script module ", assets::quoteForLog(modName),
                                " failed: ", detail::formatScriptError(scriptRes.error()));
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

        auto it = _instanceHandles.find(e);
        if (it != _instanceHandles.end() && _vm.hasVariable("_internal", "Dispatcher")) {
            auto tryReloadHandle = _vm.makeCallHandle("tryCallReload(_)");
            _vm.ensureSlots(2);
            _vm.getVariable("_internal", "Dispatcher", 0);
            _vm.setSlot(1, it->second.get());
            recordScriptResult(e, _vm.call(tryReloadHandle.get()), "onReload()");
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

    notifyVmReset(VmResetPhase::Before);
    // Every script gets a new chance with the new code.
    _failures.clear();
    _reportedModuleProblems.clear();
    releaseActiveHandles();
    recreateVmAndModules(modifiedModules);
    reinstantiateActiveScripts();
    notifyVmReset(VmResetPhase::After);
    return true;
}

}  // namespace systems
