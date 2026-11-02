/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** AnimationPlugin implementation.
*/

#include "systems/render/AnimationPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "render/RenderPlugin.hpp"
#include "systems/render/AnimationSystem.hpp"

namespace systems {

void AnimationPlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
}

void AnimationPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    AnimationSystem::update(engine.registry(), time.deltaTime);
    _parallaxSystem.update(engine.registry(), time.deltaTime);
}

}  // namespace systems
