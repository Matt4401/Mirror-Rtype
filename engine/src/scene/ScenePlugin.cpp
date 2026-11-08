#include "scene/ScenePlugin.hpp"

#include <exception>
#include <stdexcept>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "scene/SceneManager.hpp"
#include "scene/SceneTypes.hpp"

namespace scene {

void ScenePlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    // Not needed, but when there is one, scenes must exit while its hooks still cascade to the children.
    builder.optionally<hierarchy::HierarchyPlugin>();
    builder.provides<SceneManager>();
}

void ScenePlugin::init(engine::Engine& engine) {
    _manager->attach(engine);
    if (!engine.mediator().registerService<SceneManager>(_manager)) {
        throw std::logic_error("ScenePlugin: a SceneManager service is already registered");
    }
}

void ScenePlugin::preUpdate(engine::Engine& /*engine*/) { _manager->applyTransitions(); }

void ScenePlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    _manager->runStage(SceneStage::FixedUpdate, engine, time);
}

void ScenePlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    _manager->runStage(SceneStage::Update, engine, time);
}

void ScenePlugin::lateUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    _manager->runStage(SceneStage::LateUpdate, engine, time);
}

void ScenePlugin::shutdown(engine::Engine& engine) {
    // A throwing hook must not keep the manager registered or bound to the engine: finish, then report it.
    std::exception_ptr error;
    try {
        _manager->clear();
    } catch (...) {
        error = std::current_exception();
    }
    engine.mediator().removeService<SceneManager>();
    _manager->detach();
    if (error) {
        std::rethrow_exception(error);
    }
}

}  // namespace scene
