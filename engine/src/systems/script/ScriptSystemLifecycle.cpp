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

#include "systems/script/ScriptSystem.hpp"

extern "C" {
#include <wren.h>
}

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/WrenVM.hpp"

namespace systems {

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

WrenHandle* ScriptSystem::createScriptInstanceHandle(ecs::Entity e, const ecs::ScriptComponent& script) {
    ::WrenVM* raw = _vm.raw();
    if (!_vm.hasVariable(script.moduleName, script.className)) {
        std::cerr << "[ScriptSystem] Class " << script.className << " not found in " << script.moduleName << "\n";
        return nullptr;
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
        return nullptr;
    }
    return wrenGetSlotHandle(raw, 0);
}

void ScriptSystem::instantiateScript(ecs::Entity e, ecs::ScriptComponent& script, bool isReload) {
    if (script.initialized || !ensureScriptModuleLoaded(script.moduleName)) {
        return;
    }

    script.instanceHandle = createScriptInstanceHandle(e, script);
    if (script.instanceHandle == nullptr) {
        return;
    }
    script.initialized = true;

    if (!isReload && _vm.hasVariable("_internal", "Dispatcher")) {
        ::WrenVM* raw = _vm.raw();
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

void ScriptSystem::triggerDestroyHook(ecs::Entity e) {
    if (_registry == nullptr || !_registry->getComponents<ecs::ScriptComponent>().contains(e)) {
        return;
    }
    auto& script = _registry->getComponents<ecs::ScriptComponent>().get(e);
    if (script.instanceHandle == nullptr || !script.initialized) {
        return;
    }

    if (_vm.hasVariable("_internal", "Dispatcher")) {
        ::WrenVM* raw = _vm.raw();
        wrenEnsureSlots(raw, 2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        wrenSetSlotHandle(raw, 1, static_cast<WrenHandle*>(script.instanceHandle));
        auto tryDestroyHandle = _vm.makeCallHandle("tryCallDestroy(_)");
        wrenCall(raw, tryDestroyHandle.get());
    }

    _vm.releaseHandle(static_cast<WrenHandle*>(script.instanceHandle));
    script.instanceHandle = nullptr;
    script.initialized = false;
}

void ScriptSystem::queueDestroyEntity(ecs::Entity e) {
    _pendingDestroyEntities.push_back(e);
    _commandBuffer.deferDestroy(e);
}

}  // namespace systems
