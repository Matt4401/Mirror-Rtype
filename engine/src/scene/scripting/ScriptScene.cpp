#include "scene/scripting/ScriptScene.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include "core/Engine.hpp"
#include "ecs/time/Time.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneTypes.hpp"
#include "scene/scripting/SceneScriptModule.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenTypes.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace scene {

namespace {

/** @brief Name of the Wren class dispatching the optional hooks. */
constexpr std::string_view HOOKS_CLASS = "SceneHooks_";

}  // namespace

ScriptScene::ScriptScene(std::string name, std::string module, std::string className, ScriptSceneOptions options,
                         systems::ScriptSystem& scripts)
    : _name(std::move(name)),
      _module(std::move(module)),
      _className(std::move(className)),
      _options(options),
      _scripts(&scripts) {}

bool ScriptScene::build() {
    scripting::WrenVM& vm = _scripts->getVm();
    if (!vm.hasModule(SCENE_MODULE_NAME)) {
        if (auto const loaded = vm.interpret(SCENE_MODULE_NAME, SCENE_MODULE_SOURCE); !loaded) {
            std::cerr << "[ScriptScene] Cannot load " << SCENE_MODULE_NAME << ": " << loaded.error() << "\n";
            return false;
        }
    }
    if (!_scripts->loadModule(_module)) {
        return false;
    }
    {
        // The constructor may already spawn entities or wait: it runs as the scene.
        const systems::ScriptOwner previous = _scripts->setCurrentOwner(sceneOwner(_id));
        _instance = vm.newInstance(_module, _className);
        (void)_scripts->setCurrentOwner(previous);
    }
    if (!_instance) {
        std::cerr << "[ScriptScene] Cannot build " << _className << " from " << _module << "\n";
        return false;
    }
    _enterHook = vm.makeCallHandle("enter(_)");
    _exitHook = vm.makeCallHandle("exit(_)");
    _pauseHook = vm.makeCallHandle("pause(_)");
    _resumeHook = vm.makeCallHandle("resume(_)");
    if (_options.hasUpdate) {
        _updateMethod = vm.makeCallHandle("update(_)");
    }
    if (_options.hasFixedUpdate) {
        _fixedUpdateMethod = vm.makeCallHandle("fixedUpdate(_)");
    }
    return true;
}

void ScriptScene::onEnter(SceneContext& context) {
    _id = context.id();
    _active = true;
    if (!build()) {
        return;
    }
    if (_options.hasUpdate) {
        context.addSystem(SceneStage::Update, [this](engine::Engine& /*engine*/, const ecs::time::Time& time) {
            callStep(_updateMethod, time.deltaTime);
        });
    }
    if (_options.hasFixedUpdate) {
        context.addSystem(SceneStage::FixedUpdate, [this](engine::Engine& /*engine*/, const ecs::time::Time& time) {
            callStep(_fixedUpdateMethod, time.deltaTime);
        });
    }
    callHook(_enterHook);
}

void ScriptScene::onExit(SceneContext& /*context*/) {
    callHook(_exitHook);
    _scripts->cancelFibers(sceneOwner(_id));
    releaseHandles();
    _active = false;
}

void ScriptScene::onPause(SceneContext& /*context*/) { callHook(_pauseHook); }

void ScriptScene::onResume(SceneContext& /*context*/) { callHook(_resumeHook); }

void ScriptScene::releaseHandles() {
    _instance.reset();
    _enterHook.reset();
    _exitHook.reset();
    _pauseHook.reset();
    _resumeHook.reset();
    _updateMethod.reset();
    _fixedUpdateMethod.reset();
}

void ScriptScene::rebuildAfterReload() {
    if (_active && !_instance) {
        (void)build();
    }
}

void ScriptScene::callHook(const scripting::detail::UniqueWrenHandle& hook) {
    if (!_instance || !hook) {
        return;
    }
    scripting::WrenVM& vm = _scripts->getVm();
    const systems::ScriptOwner previous = _scripts->setCurrentOwner(sceneOwner(_id));
    vm.ensureSlots(2);
    vm.getVariable(SCENE_MODULE_NAME, HOOKS_CLASS, 0);
    vm.setSlot(1, _instance.get());
    if (vm.call(hook.get()) != WREN_RESULT_SUCCESS) {
        std::cerr << "[ScriptScene] A hook of scene '" << _name << "' failed\n";
    }
    (void)_scripts->setCurrentOwner(previous);
}

void ScriptScene::callStep(const scripting::detail::UniqueWrenHandle& method, float dt) {
    if (!_instance || !method) {
        return;
    }
    const systems::ScriptOwner previous = _scripts->setCurrentOwner(sceneOwner(_id));
    if (_scripts->getVm().call(method, _instance.get(), static_cast<double>(dt)) != WREN_RESULT_SUCCESS) {
        std::cerr << "[ScriptScene] update of scene '" << _name << "' failed\n";
    }
    (void)_scripts->setCurrentOwner(previous);
}

}  // namespace scene
