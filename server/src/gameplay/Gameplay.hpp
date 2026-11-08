/**
 * @file Gameplay.hpp
 * @brief Adds the R-Type gameplay plugins to a room engine.
 */

#pragma once

#include <memory>

#include "core/Engine.hpp"
#include "level/Level.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Adds the R-Type gameplay plugins to an engine, before `Engine::init()`.
 *
 * The engine must already have `systems::PhysicsPlugin`, `hierarchy::HierarchyPlugin` and `timeline::TimelinePlugin`.
 *
 * @param engine Room engine.
 * @param level Level to play, shared by every room; `nullptr` plays no level (no enemy spawns).
 */
void addGameplayPlugins(engine::Engine& engine, const std::shared_ptr<const level::Level>& level);

}  // namespace rtype::server::gameplay
