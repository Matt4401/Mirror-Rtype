/**
 * @file ScriptSystemBindings.cpp
 * @brief Foreign method bindings and built-in core module loader for ScriptSystem.
 */

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "input/InputState.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace systems {

namespace {

/**
 * @brief Foreign static method `Engine.registerWait(fiber, seconds)` callback.
 * @param call ForeignCall execution context.
 */
void engineRegisterWait(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        return;
    }
    auto fiber = call.arg<scripting::detail::UniqueWrenHandle>(1);
    auto seconds = call.arg<float>(2);
    if (!fiber.has_value() || !seconds.has_value()) {
        return;
    }
    scriptSystem->addSleepingFiber(std::move(*fiber), *seconds);
}

/**
 * @brief Foreign static method `Engine.spawnPrefab(name, x, y)` callback.
 * @param call ForeignCall execution context.
 */
void engineSpawnPrefab(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        return;
    }
    auto const prefabName = call.arg<std::string_view>(1);
    auto const x = call.arg<double>(2);
    auto const y = call.arg<double>(3);
    if (!prefabName.has_value() || !x.has_value() || !y.has_value()) {
        return;
    }

    std::string const name(*prefabName);
    double const posX = *x;
    double const posY = *y;
    scriptSystem->getCommandBuffer().deferSpawn(
        [name, posX, posY]([[maybe_unused]] ecs::Entity e, [[maybe_unused]] ecs::Registry& reg) {
            // Placeholder prefab spawner
        });
}

/**
 * @brief Foreign static method `Entity.destroyEntity(id)` callback.
 * @param call ForeignCall execution context.
 */
void entityDestroy(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        return;
    }
    auto const e = call.arg<ecs::Entity>(1);
    if (!e.has_value()) {
        return;
    }
    scriptSystem->queueDestroyEntity(*e);
}

/**
 * @brief Foreign static method `World.getEntitiesWithTag(tag)` callback.
 * @param call ForeignCall execution context.
 */
void worldGetEntitiesWithTag(scripting::ForeignCall& call) { call.retList(); }

/**
 * @brief Foreign static method `World.getNearestWithTag(x, y, tag)` callback.
 * @param call ForeignCall execution context.
 */
void worldGetNearestWithTag(scripting::ForeignCall& call) { call.retNull(); }

/**
 * @brief Foreign static method `World.getEntitiesInRadius(x, y, radius)` callback.
 * @param call ForeignCall execution context.
 */
void worldGetEntitiesInRadius(scripting::ForeignCall& call) { call.retList(); }

/**
 * @brief Resolves the InputState component matching playerIndex.
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
    return matched;
}

/**
 * @brief Foreign static method `Input.isDown(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputIsDown(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(false);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr && inputState->isDown(*actionName));
}

/**
 * @brief Foreign static method `Input.isJustPressed(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputIsJustPressed(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(false);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr && inputState->isJustPressed(*actionName));
}

/**
 * @brief Foreign static method `Input.isJustReleased(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputIsJustReleased(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(false);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(false);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr && inputState->isJustReleased(*actionName));
}

/**
 * @brief Foreign static method `Input.getAxis(playerIndex, action)` callback.
 * @param call ForeignCall execution context.
 */
void inputGetAxis(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(0.0F);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    auto const actionName = call.arg<std::string_view>(2);
    if (!playerIndex.has_value() || !actionName.has_value()) {
        call.ret(0.0F);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr ? inputState->getAxis(*actionName) : 0.0F);
}

}  // namespace

void ScriptSystem::registerCoreBindings(scripting::WrenVM& vm) {
    vm.registerForeignMethod<&engineRegisterWait>("core/Engine", "Engine", true, "registerWait(_,_)");
    vm.registerForeignMethod<&engineSpawnPrefab>("core/Engine", "Engine", true, "spawnPrefab(_,_,_)");
    vm.registerForeignMethod<&entityDestroy>("core/Entity", "Entity", true, "destroyEntity(_)");
    vm.registerForeignMethod<&worldGetEntitiesWithTag>("core/World", "World", true, "getEntitiesWithTag(_)");
    vm.registerForeignMethod<&worldGetNearestWithTag>("core/World", "World", true, "getNearestWithTag(_,_,_)");
    vm.registerForeignMethod<&worldGetEntitiesInRadius>("core/World", "World", true, "getEntitiesInRadius(_,_,_)");
    vm.registerForeignMethod<&inputIsDown>("core/Input", "Input", true, "isDown(_,_)");
    vm.registerForeignMethod<&inputIsJustPressed>("core/Input", "Input", true, "isJustPressed(_,_)");
    vm.registerForeignMethod<&inputIsJustReleased>("core/Input", "Input", true, "isJustReleased(_,_)");
    vm.registerForeignMethod<&inputGetAxis>("core/Input", "Input", true, "getAxis(_,_)");
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
