#include "scene/scripting/SceneScriptingPlugin.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"
#include "scene/scripting/SceneScriptModule.hpp"
#include "scene/scripting/ScriptScene.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace scene {

namespace {

/** @brief Name of the Wren class exposing the scene stack. */
constexpr std::string_view SCENE_CLASS = "Scene";

/**
 * @brief Gets the scene manager of the engine running a foreign call.
 * @param call Foreign call.
 * @return The manager, or null outside a running bridge.
 */
SceneManager* managerOf(scripting::ForeignCall& call) {
    auto* scripts = call.userData<systems::ScriptSystem>();
    if (scripts == nullptr || scripts->getRegistry() == nullptr ||
        !scripts->getRegistry()->hasResource<SceneScriptBridge>()) {
        return nullptr;
    }
    return scripts->getRegistry()->getResource<SceneScriptBridge>().manager;
}

/**
 * @brief Resolves the scene named by the first argument of a foreign call.
 * @param call Foreign call.
 * @param manager Manager to search.
 * @return Its id.
 * @throws std::invalid_argument if the argument is not a string or names no scene (aborts the fiber).
 */
SceneId sceneArgument(scripting::ForeignCall& call, const SceneManager& manager) {
    auto const name = call.arg<std::string_view>(1);
    if (!name) {
        throw std::invalid_argument("Scene: the scene name must be a string");
    }
    const std::optional<SceneId> id = manager.find(*name);
    if (!id) {
        throw std::invalid_argument("Scene: unknown scene '" + std::string(*name) + "'");
    }
    return *id;
}

/**
 * @brief `Scene.switchTo(name)`, `Scene.push(name)` and `Scene.replace(name)`.
 * @tparam Transition Member of `SceneManager` queueing the transition.
 * @param call Foreign call.
 */
template <void (SceneManager::*Transition)(SceneId)>
void sceneTransition(scripting::ForeignCall& call) {
    SceneManager* manager = managerOf(call);
    if (manager != nullptr) {
        (manager->*Transition)(sceneArgument(call, *manager));
    }
}

/**
 * @brief `Scene.pop()`.
 * @param call Foreign call.
 */
void scenePop(scripting::ForeignCall& call) {
    if (SceneManager* manager = managerOf(call); manager != nullptr) {
        manager->pop();
    }
}

/**
 * @brief `Scene.current`: name of the top scene, or null.
 * @param call Foreign call.
 */
void sceneCurrent(scripting::ForeignCall& call) {
    const SceneManager* manager = managerOf(call);
    if (manager == nullptr || manager->top() == INVALID_SCENE) {
        call.retNull();
        return;
    }
    call.ret(std::string(manager->scene(manager->top()).name()));
}

/**
 * @brief `Scene.isActive(name)` and `Scene.isRunning(name)`.
 * @tparam Query Member of `SceneManager` answering the question.
 * @param call Foreign call.
 */
template <bool (SceneManager::*Query)(SceneId) const noexcept>
void sceneQuery(scripting::ForeignCall& call) {
    const SceneManager* manager = managerOf(call);
    call.ret(manager != nullptr && (manager->*Query)(sceneArgument(call, *manager)));
}

/**
 * @brief Binds the foreign methods of `core/Scene`; also replayed on the VM of each hot reload.
 * @param vm VM to bind.
 */
void bindSceneModule(scripting::WrenVM& vm) {
    vm.registerForeignMethod<&sceneTransition<&SceneManager::switchTo>>(SCENE_MODULE_NAME, SCENE_CLASS, true,
                                                                        "switchTo(_)");
    vm.registerForeignMethod<&sceneTransition<&SceneManager::push>>(SCENE_MODULE_NAME, SCENE_CLASS, true, "push(_)");
    vm.registerForeignMethod<&sceneTransition<&SceneManager::replace>>(SCENE_MODULE_NAME, SCENE_CLASS, true,
                                                                       "replace(_)");
    vm.registerForeignMethod<&scenePop>(SCENE_MODULE_NAME, SCENE_CLASS, true, "pop()");
    vm.registerForeignMethod<&sceneCurrent>(SCENE_MODULE_NAME, SCENE_CLASS, true, "current");
    vm.registerForeignMethod<&sceneQuery<&SceneManager::isActive>>(SCENE_MODULE_NAME, SCENE_CLASS, true, "isActive(_)");
    vm.registerForeignMethod<&sceneQuery<&SceneManager::isRunning>>(SCENE_MODULE_NAME, SCENE_CLASS, true,
                                                                    "isRunning(_)");
}

}  // namespace

void SceneScriptingPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ScenePlugin>();
    builder.require<scripting::ScriptPlugin>();
}

void SceneScriptingPlugin::init(engine::Engine& engine) {
    _manager = &engine.getPlugin<ScenePlugin>().manager();
    _scripts = &engine.getPlugin<scripting::ScriptPlugin>().scriptSystem();
    engine.registry().emplaceResource<SceneScriptBridge>(SceneScriptBridge{.manager = _manager});

    _scripts->addModuleSource(std::string(SCENE_MODULE_NAME), std::string(SCENE_MODULE_SOURCE));
    bindSceneModule(_scripts->getVm());
    _scripts->addBinderReplay(&bindSceneModule);
    _scripts->setSpawnDecorator(&SceneScriptingPlugin::adoptSpawn, this);
    _resetListener = _scripts->addVmResetListener([this](systems::VmResetPhase phase) {
        for (ScriptScene* scriptScene : _scriptScenes) {
            if (phase == systems::VmResetPhase::Before) {
                scriptScene->releaseHandles();
            } else {
                scriptScene->rebuildAfterReload();
            }
        }
    });
}

void SceneScriptingPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_scripts != nullptr) {
        if (_resetListener) {
            _scripts->removeVmResetListener(*_resetListener);
        }
        if (_scripts->spawnDecoratorContext() == this) {
            _scripts->setSpawnDecorator(nullptr, nullptr);
        }
    }
    _resetListener.reset();
    // The manager stays readable until the scenes exit (ScenePlugin shuts down after this plugin).
}

SceneId SceneScriptingPlugin::registerScriptScene(engine::Engine& engine, std::string name, std::string module,
                                                  std::string className, ScriptSceneOptions options) {
    auto scriptScene = std::make_unique<ScriptScene>(std::move(name), std::move(module), std::move(className), options,
                                                     engine.getPlugin<scripting::ScriptPlugin>().scriptSystem());
    SceneManager& manager = engine.getPlugin<ScenePlugin>().manager();
    const SceneId id = manager.add(std::move(scriptScene));
    _scriptScenes.push_back(&dynamic_cast<ScriptScene&>(manager.scene(id)));
    return id;
}

void SceneScriptingPlugin::adoptSpawn(void* context, ecs::Registry& registry, ecs::Entity spawned,
                                      systems::ScriptOwner owner) {
    const auto* self = static_cast<const SceneScriptingPlugin*>(context);
    if (self == nullptr || self->_manager == nullptr) {
        return;
    }
    const SceneManager& manager = *self->_manager;

    std::optional<SceneId> scene = sceneOfOwner(owner);
    if (!scene && (owner & systems::ScriptSystem::EXTENSION_OWNER_BIT) == 0) {
        const auto entity = static_cast<ecs::Entity>(owner);
        if (const auto* members = static_cast<const ecs::Registry&>(registry).findComponents<SceneMember>();
            members != nullptr && members->contains(entity)) {
            scene = members->get(entity).id;
        }
    }
    if (!scene) {
        const SceneId target = manager.spawnTarget();
        if (target == INVALID_SCENE) {
            return;
        }
        scene = target;
    }
    if (!manager.isActive(*scene)) {
        // Its scene exited between the request and the flush: the entity would never be cleaned up.
        registry.killEntity(spawned);
        return;
    }
    registry.addComponent(spawned, SceneMember{.id = *scene});
}

}  // namespace scene
