/**
 * @file ScriptPlugin.cpp
 * @brief Implementation of ScriptPlugin.
 */

#include "scripting/ScriptPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"

namespace scripting {

void ScriptPlugin::build(engine::PluginBuilder& builder) { builder.require<engine::CorePlugin>(); }

void ScriptPlugin::init(engine::Engine& engine) { _scriptSystem.init(engine.registry(), engine.eventBus()); }

void ScriptPlugin::update(engine::Engine& /*engine*/, const ecs::time::Time& time) {
    _scriptSystem.update(time.deltaTime);
}

void ScriptPlugin::shutdown(engine::Engine& /*engine*/) {}

}  // namespace scripting
