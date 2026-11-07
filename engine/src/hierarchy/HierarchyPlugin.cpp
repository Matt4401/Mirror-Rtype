#include "hierarchy/HierarchyPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "hierarchy/TransformPropagation.hpp"

namespace hierarchy {

HierarchyPlugin::HierarchyPlugin() = default;

HierarchyPlugin::~HierarchyPlugin() = default;

void HierarchyPlugin::init(engine::Engine& engine) { _hooks = std::make_unique<HierarchyHooks>(engine.registry()); }

void HierarchyPlugin::lateUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    propagateTransforms(engine.registry());
}

void HierarchyPlugin::shutdown(engine::Engine& /*engine*/) { _hooks.reset(); }

}  // namespace hierarchy
