#include "CarViewPlugin.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#include "Arena.hpp"
#include "Shapes.hpp"
#include "components/Renderable.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "render/RenderPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::client {

namespace {

constexpr ecs::Rgba WHEEL_COLOR{.r = 85, .g = 88, .b = 96, .a = 255};
constexpr ecs::Rgba HEADLIGHT_COLOR{.r = 255, .g = 240, .b = 170, .a = 255};
/** @brief The cabin is the player color, darkened by this factor. */
constexpr float CABIN_SHADE = 0.6F;

constexpr int LAYER_WHEELS = 10;
constexpr int LAYER_CABIN = 11;
constexpr int LAYER_BODY = 12;
constexpr int LAYER_LIGHT = 13;

ecs::Rgba shade(const Rgb color, const float factor) {
    return ecs::Rgba{
        .r = static_cast<std::uint8_t>(static_cast<float>(color.r) * factor),
        .g = static_cast<std::uint8_t>(static_cast<float>(color.g) * factor),
        .b = static_cast<std::uint8_t>(static_cast<float>(color.b) * factor),
        .a = 255,
    };
}

/** @brief A part is placed and sized by the script: it only needs a color and a layer here. */
ecs::Entity spawnPart(ecs::Registry& registry, const ecs::Transform& car, const ecs::Rgba color, const int layer) {
    const ecs::Entity part = registry.spawnEntity();
    registry.addComponent(part, ecs::Transform{.x = car.x, .y = car.y, .scaleX = 0.0F, .scaleY = 0.0F});
    registry.addComponent(part, makeRenderable(color, layer));
    return part;
}

void killPart(ecs::Registry& registry, const ecs::Entity part) {
    if (part != 0 && registry.isAlive(part)) {
        registry.killEntity(part);
    }
}

}  // namespace

void CarViewPlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<scripting::ScriptPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

void CarViewPlugin::init(engine::Engine& engine) {
    if (!game::installScriptBindings(engine.getPlugin<scripting::ScriptPlugin>().scriptSystem())) {
        throw std::runtime_error("CarViewPlugin: Wren refused the RLSS component bindings");
    }
    auto& events = engine.eventBus();
    _spawned =
        events.subscribe<ecs::net::ReplicatedEntitySpawned>([&engine](const ecs::net::ReplicatedEntitySpawned& event) {
            ecs::Registry& registry = engine.registry();
            if (event.kind != game::KIND_CAR || !registry.hasComponent<ecs::Transform>(event.entity)) {
                return;
            }
            const ecs::Transform car = registry.getComponents<ecs::Transform>().get(event.entity);
            const auto& looks = registry.getComponents<game::CarLook>();
            const Rgb color = playerColor(looks.contains(event.entity) ? looks.get(event.entity).color : 0);
            registry.addComponent(event.entity,
                                  game::CarParts{
                                      .body = spawnPart(registry, car, shade(color, 1.0F), LAYER_BODY),
                                      .cabin = spawnPart(registry, car, shade(color, CABIN_SHADE), LAYER_CABIN),
                                      .rearWheel = spawnPart(registry, car, WHEEL_COLOR, LAYER_WHEELS),
                                      .frontWheel = spawnPart(registry, car, WHEEL_COLOR, LAYER_WHEELS),
                                      .headlight = spawnPart(registry, car, HEADLIGHT_COLOR, LAYER_LIGHT),
                                  });
            registry.addComponent(event.entity, ecs::ScriptComponent{
                                                    .moduleName = std::string(game::CAR_VIEW_SCRIPT_MODULE),
                                                    .className = "CarView",
                                                    .hasUpdate = true,
                                                });
        });
    _despawned = events.subscribe<ecs::net::ReplicatedEntityDespawned>(
        [&engine](const ecs::net::ReplicatedEntityDespawned& event) {
            ecs::Registry& registry = engine.registry();
            auto& allParts = registry.getComponents<game::CarParts>();
            if (!allParts.contains(event.entity)) {
                return;
            }
            const game::CarParts parts = allParts.get(event.entity);
            for (const ecs::Entity part :
                 {parts.body, parts.cabin, parts.rearWheel, parts.frontWheel, parts.headlight}) {
                killPart(registry, part);
            }
            // The engine kills the car right after this event: drop its script instance first.
            engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().triggerDestroyHook(event.entity);
        });
}

void CarViewPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_spawned) {
        events.unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<ecs::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
}

}  // namespace rlss::client
