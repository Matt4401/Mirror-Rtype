/**
 * @file ScriptSystemBindings.cpp
 * @brief Foreign method bindings and built-in core module loader for ScriptSystem.
 */

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "log/Logger.hpp"
#include "assets/AssetPath.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/SpatialQueries.hpp"
#include "input/InputState.hpp"
#include "prefabs/PrefabManager.hpp"
#include "scripting/CoreModules.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/LogTag.hpp"
#include "systems/script/ScriptSystem.hpp"
#include "systems/script/detail/ScriptSupport.hpp"

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

    auto const* manager = scriptSystem->getPrefabManager();
    if (manager == nullptr) {
        engine::log::warning(scripting::detail::LOG_TAG, "Engine.spawnPrefab(", assets::quoteForLog(*prefabName),
                            "): no PrefabManager set");
        return;
    }
    ScriptSystem::SpawnDecorator const decorator = scriptSystem->spawnDecorator();
    if (decorator == nullptr) {
        (void)manager->spawn(*prefabName, scriptSystem->getCommandBuffer(), {{"x", *x}, {"y", *y}});
        return;
    }
    (void)manager->spawn(
        *prefabName, scriptSystem->getCommandBuffer(), {{"x", *x}, {"y", *y}},
        [decorator, context = scriptSystem->spawnDecoratorContext(), owner = scriptSystem->currentOwner()](
            ecs::Entity e, ecs::Registry& registry) { decorator(context, registry, e, owner); });
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
 * @brief Returns the query buffer to Wren as a list of entity ids, skipping entities pending destruction.
 * @param call ForeignCall execution context.
 * @param scriptSystem Owner of the buffer and of the pending-destroy queue.
 */
void retEntityIdList(scripting::ForeignCall& call, ScriptSystem& scriptSystem) {
    call.retList();
    for (ecs::Entity const e : scriptSystem.getQueryBuffer()) {
        if (!scriptSystem.isPendingDestroy(e)) {
            call.listAppend(static_cast<double>(e));
        }
    }
}

/**
 * @brief Foreign static method `World.idsWithTag_(tag)` callback, backing `World.getEntitiesWithTag`.
 * @param call ForeignCall execution context.
 */
void worldIdsWithTag(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    auto const tag = call.arg<std::string_view>(1);
    if (!tag.has_value()) {
        return;
    }
    if (scriptSystem == nullptr || scriptSystem->getRegistry() == nullptr) {
        call.retList();
        return;
    }
    ecs::entitiesWithTag(*scriptSystem->getRegistry(), *tag, scriptSystem->getQueryBuffer());
    retEntityIdList(call, *scriptSystem);
}

/**
 * @brief Foreign static method `World.nearestIdWithTag_(x, y, tag)` callback, backing `World.getNearestWithTag`.
 * @param call ForeignCall execution context.
 */
void worldNearestIdWithTag(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    auto const x = call.arg<float>(1);
    auto const y = call.arg<float>(2);
    auto const tag = call.arg<std::string_view>(3);
    if (!x.has_value() || !y.has_value() || !tag.has_value()) {
        return;
    }
    if (scriptSystem == nullptr || scriptSystem->getRegistry() == nullptr) {
        call.retNull();
        return;
    }
    auto const nearest =
        ecs::nearestWithTag(*scriptSystem->getRegistry(), *x, *y, *tag, scriptSystem->getPendingDestroyEntities());
    if (nearest.has_value()) {
        call.ret(static_cast<double>(*nearest));
    } else {
        call.retNull();
    }
}

/**
 * @brief Foreign static method `World.idsInRadius_(x, y, radius)` callback, backing `World.getEntitiesInRadius`.
 * @param call ForeignCall execution context.
 */
void worldIdsInRadius(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    auto const x = call.arg<float>(1);
    auto const y = call.arg<float>(2);
    auto const radius = call.arg<float>(3);
    if (!x.has_value() || !y.has_value() || !radius.has_value()) {
        return;
    }
    if (scriptSystem == nullptr || scriptSystem->getRegistry() == nullptr) {
        call.retList();
        return;
    }
    ecs::entitiesInRadius(*scriptSystem->getRegistry(), *x, *y, *radius, scriptSystem->getQueryBuffer());
    retEntityIdList(call, *scriptSystem);
}

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
    const auto& denseStates = registry->getComponents<input::InputState>().getDense();
    for (const auto& state : denseStates) {
        if (state.playerIndex == playerIndex) {
            return &state;
        }
    }
    return nullptr;
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

/**
 * @brief Returns one field of a player's mouse state to Wren.
 * @tparam Field MouseState field to return.
 * @param call ForeignCall execution context.
 */
template <float input::MouseState::* Field>
void inputMouseField(scripting::ForeignCall& call) {
    auto* scriptSystem = call.userData<ScriptSystem>();
    if (scriptSystem == nullptr) {
        call.ret(0.0F);
        return;
    }
    auto const playerIndex = call.arg<int>(1);
    if (!playerIndex.has_value()) {
        call.ret(0.0F);
        return;
    }
    const auto* inputState = resolvePlayerInputState(scriptSystem->getRegistry(), *playerIndex);
    call.ret(inputState != nullptr ? inputState->mouse.*Field : 0.0F);
}

}  // namespace

void ScriptSystem::registerCoreBindings(scripting::WrenVM& vm) {
    vm.registerForeignMethod<&engineRegisterWait>("core/Engine", "Engine", true, "registerWait(_,_)");
    vm.registerForeignMethod<&engineSpawnPrefab>("core/Engine", "Engine", true, "spawnPrefab(_,_,_)");
    vm.registerForeignMethod<&entityDestroy>("core/Entity", "Entity", true, "destroyEntity(_)");
    vm.registerForeignMethod<&worldIdsWithTag>("core/World", "World", true, "idsWithTag_(_)");
    vm.registerForeignMethod<&worldNearestIdWithTag>("core/World", "World", true, "nearestIdWithTag_(_,_,_)");
    vm.registerForeignMethod<&worldIdsInRadius>("core/World", "World", true, "idsInRadius_(_,_,_)");
    vm.registerForeignMethod<&inputIsDown>("core/Input", "Input", true, "isDown(_,_)");
    vm.registerForeignMethod<&inputIsJustPressed>("core/Input", "Input", true, "isJustPressed(_,_)");
    vm.registerForeignMethod<&inputIsJustReleased>("core/Input", "Input", true, "isJustReleased(_,_)");
    vm.registerForeignMethod<&inputGetAxis>("core/Input", "Input", true, "getAxis(_,_)");
    vm.registerForeignMethod<&inputMouseField<&input::MouseState::x>>("core/Input", "Input", true, "mouseX(_)");
    vm.registerForeignMethod<&inputMouseField<&input::MouseState::y>>("core/Input", "Input", true, "mouseY(_)");
    vm.registerForeignMethod<&inputMouseField<&input::MouseState::wheel>>("core/Input", "Input", true, "mouseWheel(_)");
}

void ScriptSystem::loadCoreModules() {
    _vm.setModuleLoader([extraModules = _extraModules, root = _scriptRoot](std::string_view moduleName) -> std::string {
        std::string_view const coreSource = scripting::getCoreModuleSource(moduleName);
        if (!coreSource.empty()) {
            return std::string(coreSource);
        }
        if (auto const it = extraModules->find(std::string(moduleName)); it != extraModules->end()) {
            return it->second;
        }

        std::optional<std::filesystem::path> const filename = detail::resolveScriptFile(*root, moduleName);
        if (!filename) {
            detail::logRefusedScriptModule(moduleName);
            return "";
        }
        std::ifstream file(*filename);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }
        return "";
    });

    auto const res = _vm.interpret("_internal", scripting::INTERNAL_DISPATCHER_SOURCE);
    if (!res) {
        engine::log::error(scripting::detail::LOG_TAG,
                          "cannot run the internal dispatcher: ", detail::formatScriptError(res.error()));
    }
}

}  // namespace systems
