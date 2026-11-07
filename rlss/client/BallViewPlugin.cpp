#include "BallViewPlugin.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

#include "CarViewPlugin.hpp"
#include "Shapes.hpp"
#include "components/Renderable.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::client {

namespace {

/** @brief Same radius as the physics (`BallTuning.RADIUS`). */
constexpr float BALL_RADIUS = 91.25F;
/** @brief Strips making the disc. */
constexpr int STRIPS = 22;
constexpr float SEAM_WIDTH = 6.0F;
constexpr ecs::Rgba BALL_COLOR{.r = 236, .g = 238, .b = 242, .a = 255};
constexpr ecs::Rgba SEAM_COLOR{.r = 120, .g = 128, .b = 142, .a = 255};
constexpr int LAYER_HALO = 13;
constexpr int LAYER_BALL = 14;
/** @brief The glow after a colored shot: a disc this much wider than the ball, behind it. */
constexpr float HALO_WIDTH = 16.0F;
/** @brief Halo color of a shot. */
ecs::Rgba shotColor(const game::Shot shot) {
    constexpr std::uint8_t ALPHA = 230;
    switch (shot) {
        case game::Shot::Red:
            return {.r = 255, .g = 60, .b = 60, .a = ALPHA};
        case game::Shot::Purple:
            return {.r = 180, .g = 90, .b = 255, .a = ALPHA};
        case game::Shot::Gold:
            return {.r = 255, .g = 200, .b = 40, .a = ALPHA};
        case game::Shot::None:
            break;
    }
    return {.r = 0, .g = 0, .b = 0, .a = 0};
}
constexpr int LAYER_SEAM = 15;

ecs::Entity spawnPiece(ecs::Registry& registry, const game::BallPart& part, const float width, const float height,
                       const ecs::Rgba color, const int layer) {
    const ecs::Entity piece = registry.spawnEntity();
    registry.addComponent(piece, ecs::Transform{
                                     .x = 0.0F,
                                     .y = 0.0F,
                                     .scaleX = width / PLACEHOLDER_SIZE,
                                     .scaleY = height / PLACEHOLDER_SIZE,
                                 });
    registry.addComponent(piece, makeRenderable(color, layer));
    registry.addComponent(piece, ecs::Tag{.name = std::string(game::BALL_PART_TAG)});
    registry.addComponent(piece, part);
    return piece;
}

}  // namespace

void BallViewPlugin::build(engine::PluginBuilder& builder) {
    builder.require<CarViewPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

void BallViewPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _spawned = events.subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) {
            if (event.kind != game::KIND_BALL) {
                return;
            }
            ecs::Registry& registry = engine.registry();
            std::vector<ecs::Entity>& pieces = _pieces[event.entity];
            const float strip = 2.0F * BALL_RADIUS / static_cast<float>(STRIPS);
            for (int i = 0; i < STRIPS; ++i) {
                const float middle = -BALL_RADIUS + ((static_cast<float>(i) + 0.5F) * strip);
                const float halfWidth = std::sqrt((BALL_RADIUS * BALL_RADIUS) - (middle * middle));
                pieces.push_back(spawnPiece(
                    registry, game::BallPart{.parent = static_cast<std::uint32_t>(event.entity), .offsetY = middle},
                    2.0F * halfWidth, strip + 0.5F, BALL_COLOR, LAYER_BALL));
            }
            // One halo per shot color, hidden until the ball glows with it.
            const float haloRadius = BALL_RADIUS + HALO_WIDTH;
            const float haloStrip = 2.0F * haloRadius / static_cast<float>(STRIPS);
            for (const game::Shot shot : {game::Shot::Red, game::Shot::Purple, game::Shot::Gold}) {
                for (int i = 0; i < STRIPS; ++i) {
                    const float middle = -haloRadius + ((static_cast<float>(i) + 0.5F) * haloStrip);
                    const float halfWidth = std::sqrt((haloRadius * haloRadius) - (middle * middle));
                    const game::BallPart halo{
                        .parent = static_cast<std::uint32_t>(event.entity),
                        .offsetY = middle,
                        .glow = static_cast<int>(shot),
                    };
                    pieces.push_back(
                        spawnPiece(registry, halo, 2.0F * halfWidth, haloStrip + 0.5F, shotColor(shot), LAYER_HALO));
                }
            }
            for (const float angle : {0.0F, std::numbers::pi_v<float> / 2.0F}) {
                pieces.push_back(spawnPiece(
                    registry,
                    game::BallPart{.parent = static_cast<std::uint32_t>(event.entity), .angle = angle, .spins = true},
                    2.0F * BALL_RADIUS * 0.9F, SEAM_WIDTH, SEAM_COLOR, LAYER_SEAM));
            }
            registry.addComponent(event.entity, ecs::ScriptComponent{
                                                    .moduleName = std::string(game::BALL_VIEW_SCRIPT_MODULE),
                                                    .className = "BallView",
                                                    .hasUpdate = true,
                                                });
        });
    _despawned = events.subscribe<ecs::net::ReplicatedEntityDespawned>(
        [this, &engine](const ecs::net::ReplicatedEntityDespawned& event) {
            const auto it = _pieces.find(event.entity);
            if (it == _pieces.end()) {
                return;
            }
            for (const ecs::Entity piece : it->second) {
                if (engine.registry().isAlive(piece)) {
                    engine.registry().killEntity(piece);
                }
            }
            _pieces.erase(it);
            engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().triggerDestroyHook(event.entity);
        });
}

void BallViewPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_spawned) {
        events.unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<ecs::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
    _pieces.clear();
}

}  // namespace rlss::client
