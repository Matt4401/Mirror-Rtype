#include "metrics/ShipMetricsPlugin.hpp"

#include <format>

#include "components/Predicted.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"

namespace client {

void ShipMetricsPlugin::build(engine::PluginBuilder& builder) { builder.require<metrics::MetricsPlugin>(); }

void ShipMetricsPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    if (!_timer.tick(time.deltaTime)) {
        return;
    }
    ecs::Registry& registry = engine.registry();
    auto& board = registry.getResource<metrics::MetricsBoard>();
    registry.runSystem<ecs::Predicted, ecs::Transform>(
        [&board](const ecs::Entity /*entity*/, const ecs::Predicted& /*predicted*/, const ecs::Transform& transform) {
            board.set("Position", std::format("{:.0f}, {:.0f}", transform.x, transform.y));
        });
}

}  // namespace client
