/**
 * @file TimePlugin.cpp
 * @brief Implementation of TimePlugin.
 */

#include "core/plugins/TimePlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/time/Time.hpp"

namespace engine {

void TimePlugin::build(PluginBuilder& builder) { builder.require<CorePlugin>(); }

void TimePlugin::init(Engine& engine) {
    if (!engine.registry().hasResource<ecs::time::Time>()) {
        engine.registry().emplaceResource<ecs::time::Time>(ecs::time::Time{
            .deltaTime = 0.0F,
            .totalElapsedTime = 0.0F,
            .tickCount = 0,
        });
    }
}

void TimePlugin::fixedUpdate(Engine& engine, const ecs::time::Time& time) {
    if (engine.registry().hasResource<ecs::time::Time>()) {
        engine.registry().getResource<ecs::time::Time>() = time;
    }
}

void TimePlugin::update(Engine& engine, const ecs::time::Time& time) {
    if (engine.registry().hasResource<ecs::time::Time>()) {
        engine.registry().getResource<ecs::time::Time>() = time;
    }
}

}  // namespace engine
