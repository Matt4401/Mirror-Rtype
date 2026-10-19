/**
 * @file ScriptSystemBindings.cpp
 * @brief Foreign method bindings and built-in core module loader for ScriptSystem.
 */

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "systems/script/ScriptSystem.hpp"

extern "C" {
#include <wren.h>
}

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "input/InputState.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/WrenVM.hpp"

namespace systems {

namespace {

/**
 * @brief Foreign static method `Engine.registerWait(fiber, seconds)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
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

/**
 * @brief Foreign static method `Engine.spawnPrefab(name, x, y)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
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

/**
 * @brief Foreign static method `Entity.destroyEntity(id)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void entityDestroy(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    auto const e = static_cast<ecs::Entity>(wrenGetSlotDouble(rawVm, 1));
    scriptSystem->queueDestroyEntity(e);
}

/**
 * @brief Foreign static method `World.getEntitiesWithTag(tag)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void worldGetEntitiesWithTag(::WrenVM* rawVm) {
    wrenEnsureSlots(rawVm, 1);
    wrenSetSlotNewList(rawVm, 0);
}

/**
 * @brief Foreign static method `World.getNearestWithTag(x, y, tag)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void worldGetNearestWithTag(::WrenVM* rawVm) { wrenSetSlotNull(rawVm, 0); }

/**
 * @brief Foreign static method `World.getEntitiesInRadius(x, y, radius)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void worldGetEntitiesInRadius(::WrenVM* rawVm) {
    wrenEnsureSlots(rawVm, 1);
    wrenSetSlotNewList(rawVm, 0);
}

/**
 * @brief Resolves the InputState component matching playerIndex or falls back to global resource.
 * @param registry Active ECS registry.
 * @param playerIndex Logical player identifier.
 * @return Pointer to InputState or nullptr if missing.
 */
const input::InputState* resolvePlayerInputState(ecs::Registry* registry, int playerIndex) {
    if (registry == nullptr) {
        return nullptr;
    }
    const input::InputState* matched = nullptr;
    registry->runSystem<input::InputState>([&matched, playerIndex](ecs::Entity /*e*/, const input::InputState& state) {
        if (state.playerIndex == playerIndex && matched == nullptr) {
            matched = &state;
        }
    });
    if (matched != nullptr) {
        return matched;
    }
    if (registry->hasResource<input::InputState>()) {
        return &registry->getResource<input::InputState>();
    }
    return nullptr;
}

/**
 * @brief Foreign static method `Input.isDown(playerIndex, action)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void inputIsDown(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        wrenSetSlotBool(rawVm, 0, false);
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    const auto playerIndex = static_cast<int>(wrenGetSlotDouble(rawVm, 1));
    const char* actionName = wrenGetSlotString(rawVm, 2);
    if (actionName == nullptr) {
        wrenSetSlotBool(rawVm, 0, false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), playerIndex);
    wrenSetSlotBool(rawVm, 0, (inputState != nullptr) ? inputState->isDown(actionName) : false);
}

/**
 * @brief Foreign static method `Input.isJustPressed(playerIndex, action)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void inputIsJustPressed(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        wrenSetSlotBool(rawVm, 0, false);
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    const auto playerIndex = static_cast<int>(wrenGetSlotDouble(rawVm, 1));
    const char* actionName = wrenGetSlotString(rawVm, 2);
    if (actionName == nullptr) {
        wrenSetSlotBool(rawVm, 0, false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), playerIndex);
    wrenSetSlotBool(rawVm, 0, (inputState != nullptr) ? inputState->isJustPressed(actionName) : false);
}

/**
 * @brief Foreign static method `Input.isJustReleased(playerIndex, action)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void inputIsJustReleased(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        wrenSetSlotBool(rawVm, 0, false);
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    const auto playerIndex = static_cast<int>(wrenGetSlotDouble(rawVm, 1));
    const char* actionName = wrenGetSlotString(rawVm, 2);
    if (actionName == nullptr) {
        wrenSetSlotBool(rawVm, 0, false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), playerIndex);
    wrenSetSlotBool(rawVm, 0, (inputState != nullptr) ? inputState->isJustReleased(actionName) : false);
}

/**
 * @brief Foreign static method `Input.getAxis(playerIndex, action)` callback.
 * @param rawVm Raw Wren VM pointer.
 */
void inputGetAxis(::WrenVM* rawVm) {
    auto* vmWrapper = static_cast<scripting::WrenVM*>(wrenGetUserData(rawVm));
    if (vmWrapper == nullptr || vmWrapper->getUserContext() == nullptr) {
        wrenSetSlotDouble(rawVm, 0, 0.0);
        return;
    }
    auto* scriptSystem = static_cast<ScriptSystem*>(vmWrapper->getUserContext());
    const auto playerIndex = static_cast<int>(wrenGetSlotDouble(rawVm, 1));
    const char* actionName = wrenGetSlotString(rawVm, 2);
    if (actionName == nullptr) {
        wrenSetSlotDouble(rawVm, 0, 0.0);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), playerIndex);
    wrenSetSlotDouble(rawVm, 0, (inputState != nullptr) ? static_cast<double>(inputState->getAxis(actionName)) : 0.0);
}

}  // namespace

void ScriptSystem::registerCoreBindings(scripting::WrenVM& vm) {
    vm.registerForeignMethod("core/Engine", "Engine", true, "registerWait(_,_)", &engineRegisterWait);
    vm.registerForeignMethod("core/Engine", "Engine", true, "spawnPrefab(_,_,_)", &engineSpawnPrefab);
    vm.registerForeignMethod("core/Entity", "Entity", true, "destroyEntity(_)", &entityDestroy);
    vm.registerForeignMethod("core/World", "World", true, "getEntitiesWithTag(_)", &worldGetEntitiesWithTag);
    vm.registerForeignMethod("core/World", "World", true, "getNearestWithTag(_,_,_)", &worldGetNearestWithTag);
    vm.registerForeignMethod("core/World", "World", true, "getEntitiesInRadius(_,_,_)", &worldGetEntitiesInRadius);
    vm.registerForeignMethod("core/Input", "Input", true, "isDown(_,_)", &inputIsDown);
    vm.registerForeignMethod("core/Input", "Input", true, "isJustPressed(_,_)", &inputIsJustPressed);
    vm.registerForeignMethod("core/Input", "Input", true, "isJustReleased(_,_)", &inputIsJustReleased);
    vm.registerForeignMethod("core/Input", "Input", true, "getAxis(_,_)", &inputGetAxis);
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

}  // namespace systems
