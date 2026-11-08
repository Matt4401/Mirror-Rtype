/**
 * @file ScriptSystemLifecycle.cpp
 * @brief Implementation of script module compilation, instantiation, and destruction hooks.
 */

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "log/Logger.hpp"
#include "assets/AssetPath.hpp"
#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "systems/script/ScriptSystem.hpp"
#include "systems/script/detail/ScriptSupport.hpp"

namespace systems {

using scripting::detail::LOG_TAG;

namespace {

/** @brief Extension of the script files. */
constexpr std::string_view SCRIPT_EXTENSION = ".wren";

}  // namespace

namespace detail {

std::optional<std::filesystem::path> resolveScriptFile(const std::filesystem::path& root, std::string_view moduleName) {
    std::optional<std::string> normalized = assets::normalizeAssetPath(moduleName);
    if (!normalized || !assets::isConfinedAssetPath(*normalized)) {
        return std::nullopt;
    }
    if (!normalized->ends_with(SCRIPT_EXTENSION)) {
        *normalized += SCRIPT_EXTENSION;
    }
    std::filesystem::path const file = assets::fromUtf8(*normalized);
    return root.empty() ? file : root / file;
}

void logRefusedScriptModule(std::string_view moduleName) {
    engine::log::warning(LOG_TAG, "script module ", assets::quoteForLog(moduleName),
                        " refused: it must be a relative path inside the script root, without '..'");
}

}  // namespace detail

bool ScriptSystem::reportModuleProblemOnce(std::string key) {
    return _reportedModuleProblems.insert(std::move(key)).second;
}

::WrenHandle* ScriptSystem::getScriptInstance(ecs::Entity e) const noexcept {
    auto it = _instanceHandles.find(e);
    if (it != _instanceHandles.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ScriptSystem::hasScriptInstance(ecs::Entity e) const noexcept { return _instanceHandles.contains(e); }

bool ScriptSystem::ensureScriptModuleLoaded(const std::string& moduleName) {
    std::optional<std::filesystem::path> const resolved = detail::resolveScriptFile(*_scriptRoot, moduleName);
    if (!resolved) {
        if (reportModuleProblemOnce(moduleName)) {
            detail::logRefusedScriptModule(moduleName);
        }
        return false;
    }
    std::string const filename = assets::toUtf8(*resolved);

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
        if (!reportModuleProblemOnce(moduleName)) {
            return false;
        }
        engine::log::warning(LOG_TAG, "script module ", assets::quoteForLog(moduleName), ": file ",
                            assets::quoteForLog(filename, assets::MAX_PATH_LENGTH), " not found");
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    auto res = _vm.interpret(moduleName, source);
    if (!res) {
        if (!reportModuleProblemOnce(moduleName)) {
            return false;
        }
        engine::log::warning(LOG_TAG, "cannot load script module ", assets::quoteForLog(moduleName), ": ",
                            detail::formatScriptError(res.error()));
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
        if (!reportModuleProblemOnce(script.moduleName + ':' + script.className)) {
            return {};
        }
        engine::log::warning(LOG_TAG, "class ", assets::quoteForLog(script.className), " not found in script module ",
                            assets::quoteForLog(script.moduleName));
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
        const OwnerScope scope(*this, e);
        auto trySpawnHandle = _vm.makeCallHandle("tryCallSpawn(_)");
        _vm.ensureSlots(2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        _vm.setSlot(1, _instanceHandles[e].get());
        recordScriptResult(e, _vm.call(trySpawnHandle.get()), "onSpawn()");
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
        const OwnerScope scope(*this, e);
        auto tryDestroyHandle = _vm.makeCallHandle("tryCallDestroy(_)");
        _vm.ensureSlots(2);
        _vm.getVariable("_internal", "Dispatcher", 0);
        _vm.setSlot(1, it->second.get());
        if (_vm.call(tryDestroyHandle.get()) != WREN_RESULT_SUCCESS) {
            engine::log::warning(LOG_TAG, "entity ", e,
                                ": onDestroy() failed: ", detail::formatScriptError(_vm.lastError()));
        }
    }

    _instanceHandles.erase(it);
    _failures.erase(e);

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
