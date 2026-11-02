/**
 * @file ScriptSystemLifecycle.cpp
 * @brief Implementation of script module compilation, instantiation, and destruction hooks.
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace systems {

::WrenHandle* ScriptSystem::getScriptInstance(ecs::Entity e) const noexcept {
    auto it = _instanceHandles.find(e);
    if (it != _instanceHandles.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ScriptSystem::hasScriptInstance(ecs::Entity e) const noexcept { return _instanceHandles.contains(e); }

bool ScriptSystem::ensureScriptModuleLoaded(const std::string& moduleName) {
    std::string filename = moduleName;
    if (!filename.ends_with(".wren")) {
        filename += ".wren";
    }

    std::error_code ec;
    if (std::filesystem::exists(filename, ec)) {
        auto const lastWrite = std::filesystem::last_write_time(filename, ec);
        if (!ec) {
            _watchedScripts[moduleName] =
                ScriptFileWatch{.filePath = filename, .lastWriteTime = lastWrite, .lastGoodSource = ""};
        }
    }

    if (_vm.hasModule(moduleName)) {
        return true;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[ScriptSystem] Module file not found: " << filename << "\n";
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    auto res = _vm.interpret(moduleName, source);
    if (!res) {
        std::cerr << "[ScriptSystem] Failed to load module " << moduleName << ": " << res.error() << "\n";
        return false;
    }
    if (auto it = _watchedScripts.find(moduleName); it != _watchedScripts.end()) {
        it->second.lastGoodSource = std::move(source);
    } else {
        std::error_code timeEc;
        auto const lastWrite = std::filesystem::last_write_time(filename, timeEc);
        _watchedScripts[moduleName] =
            ScriptFileWatch{.filePath = filename,
                            .lastWriteTime = timeEc ? std::filesystem::file_time_type{} : lastWrite,
                            .lastGoodSource = std::move(source)};
    }
    return true;
}

scripting::detail::UniqueWrenHandle ScriptSystem::createScriptInstance(ecs::Entity e,
                                                                       const ecs::ScriptComponent& script) {
    if (!_vm.hasVariable(script.moduleName, script.className)) {
        std::cerr << "[ScriptSystem] Class " << script.className << " not found in " << script.moduleName << "\n";
        return {};
    }

    scripting::detail::UniqueWrenHandle entityArg;
    if (_vm.hasVariable("core/Entity", "Entity")) {
        entityArg = _vm.newInstanceWithHandle(_newHandle, "core/Entity", "Entity", static_cast<double>(e));
    }

    if (entityArg) {
        return _vm.newInstanceWithHandle(_newHandle, script.moduleName, script.className, entityArg.get());
    }
    return _vm.newInstanceWithHandle(_newHandle, script.moduleName, script.className, static_cast<double>(e));
}

void ScriptSystem::instantiateScript(ecs::Entity e, ecs::ScriptComponent& script, bool isReload) {
    if (script.initialized || !ensureScriptModuleLoaded(script.moduleName)) {
        return;
    }

    auto instance = createScriptInstance(e, script);
    if (!instance) {
        return;
    }

    _instanceHandles[e] = std::move(instance);
    script.initialized = true;

    if (!isReload && _vm.hasVariable("_internal", "Dispatcher")) {
        auto trySpawnHandle = _vm.makeCallHandle("tryCallSpawn(_)");
        _vm.ensureSlots(2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        _vm.setSlot(1, _instanceHandles[e].get());
        auto const res = _vm.call(trySpawnHandle.get());
        if (res != WREN_RESULT_SUCCESS) {
            std::cerr << "[ScriptSystem] Warning: tryCallSpawn failed for entity " << e << "\n";
        }
    }

    if (script.hasUpdate) {
        _updatingEntities.push_back(e);
    }
}

void ScriptSystem::triggerDestroyHook(ecs::Entity e) {
    auto it = _instanceHandles.find(e);
    if (it == _instanceHandles.end()) {
        return;
    }

    if (_vm.hasVariable("_internal", "Dispatcher")) {
        auto tryDestroyHandle = _vm.makeCallHandle("tryCallDestroy(_)");
        _vm.ensureSlots(2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        _vm.setSlot(1, it->second.get());
        auto const res = _vm.call(tryDestroyHandle.get());
        if (res != WREN_RESULT_SUCCESS) {
            std::cerr << "[ScriptSystem] Warning: tryCallDestroy failed for entity " << e << "\n";
        }
    }

    _instanceHandles.erase(it);

    if (_registry != nullptr && _registry->getComponents<ecs::ScriptComponent>().contains(e)) {
        auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
        script.initialized = false;
    }
}

void ScriptSystem::queueDestroyEntity(ecs::Entity e) {
    _pendingDestroyEntities.push_back(e);
    _commandBuffer.deferDestroy(e);
}

}  // namespace systems
