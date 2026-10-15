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
#include "scripting/CoreModules.hpp"
#include "scripting/WrenVM.hpp"

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
    scriptSystem->queueDestroyEntity(e);
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

void ScriptSystem::registerCoreBindings(scripting::WrenVM& vm) {
    vm.registerForeignMethod("core/Engine", "Engine", true, "registerWait(_,_)", &engineRegisterWait);
    vm.registerForeignMethod("core/Engine", "Engine", true, "spawnPrefab(_,_,_)", &engineSpawnPrefab);
    vm.registerForeignMethod("core/Entity", "Entity", true, "destroyEntity(_)", &entityDestroy);
    vm.registerForeignMethod("core/World", "World", true, "getEntitiesWithTag(_)", &worldGetEntitiesWithTag);
    vm.registerForeignMethod("core/World", "World", true, "getNearestWithTag(_,_,_)", &worldGetNearestWithTag);
    vm.registerForeignMethod("core/World", "World", true, "getEntitiesInRadius(_,_,_)", &worldGetEntitiesInRadius);
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
