/**
 * @file PrefabPlugin.cpp
 * @brief Implementation of PrefabPlugin.
 */

#include "prefabs/PrefabPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "prefabs/PrefabManager.hpp"

namespace prefabs {

PrefabPlugin::PrefabPlugin() : _manager(std::make_shared<PrefabManager>()) {}

PrefabPlugin::~PrefabPlugin() = default;

PrefabPlugin::PrefabPlugin(PrefabPlugin&&) noexcept = default;
PrefabPlugin& PrefabPlugin::operator=(PrefabPlugin&&) noexcept = default;

void PrefabPlugin::build(engine::PluginBuilder& builder) { builder.require<engine::CorePlugin>(); }

void PrefabPlugin::init(engine::Engine& engine) {
    if (_manager != nullptr) {
        engine.mediator().registerService<PrefabManager>(_manager);
    }
}

void PrefabPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<PrefabManager>()) {
        engine.mediator().removeService<PrefabManager>();
    }
}

}  // namespace prefabs
