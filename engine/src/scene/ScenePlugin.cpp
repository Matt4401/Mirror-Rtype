#include "scene/ScenePlugin.hpp"

#include <stdexcept>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"
#include "scene/SceneManager.hpp"
#include "scene/SceneTypes.hpp"

namespace scene {

void ScenePlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
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
    _manager->clear();
    engine.mediator().removeService<SceneManager>();
    _manager->detach();
}

}  // namespace scene
