#include "gameplay/TerrainPlugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "level/Level.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

void TerrainPlugin::build(engine::PluginBuilder& builder) {
    builder.require<systems::PhysicsPlugin>();
    builder.require<timeline::TimelinePlugin>();
}

void TerrainPlugin::init(engine::Engine& engine) {
    _bus = &engine.eventBus();
    _playedToken = _bus->subscribe<LevelEventPlayed>([this](const LevelEventPlayed& played) {
        if (played.event.type == level::EVENT_TERRAIN) {
            startSection(played.event.params);
        }
    });
    _startedToken = _bus->subscribe<LevelStarted>([this](const LevelStarted& /*started*/) { _sections.clear(); });
}

void TerrainPlugin::startSection(const ::prefabs::PrefabParams& params) {
    // Values were validated by the loader; the height is rounded to what a block's subtype can carry.
    const double units = std::round(params.get("height", 0.0) / static_cast<double>(game::TERRAIN_HEIGHT_UNIT));
    const float height = static_cast<float>(std::clamp(units, 1.0, 255.0)) * game::TERRAIN_HEIGHT_UNIT;
    const double length = std::max(params.get("length", 0.0), 0.0);
    const bool top = params.get<std::string>("side", "") == level::TERRAIN_TOP;
    _sections.push_back(Section{
        .y = top ? height / 2.0F : SCENE_HEIGHT - (height / 2.0F),
        .height = height,
        .blocksLeft = static_cast<std::size_t>(std::ceil(length / static_cast<double>(game::TERRAIN_BLOCK_WIDTH))),
        .lastBlock = std::nullopt,
    });
}

void TerrainPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    ecs::Registry& registry = engine.registry();
    const float velocity = -engine.getPlugin<timeline::TimelinePlugin>().speed();
    for (Section& section : _sections) {
        while (section.blocksLeft > 0) {
            // Right edge of the section so far: the right edge of the screen before its first block.
            float rightEdge = SCENE_WIDTH;
            if (section.lastBlock && registry.isAlive(*section.lastBlock)) {
                rightEdge = registry.getComponents<ecs::Transform>().get(*section.lastBlock).x +
                            (game::TERRAIN_BLOCK_WIDTH / 2.0F);
            }
            if (rightEdge > SCENE_WIDTH + OFFSCREEN_MARGIN) {
                break;
            }
            const ecs::Entity block = registry.spawnEntity();
            ::prefabs::buildObstacle(block, registry,
                                     {{"x", rightEdge + (game::TERRAIN_BLOCK_WIDTH / 2.0F)},
                                      {"y", section.y},
                                      {"halfWidth", game::TERRAIN_BLOCK_WIDTH / 2.0F},
                                      {"halfHeight", section.height / 2.0F}});
            // Moved at the scroll speed: kinematic, unlike a static obstacle.
            registry.getComponents<ecs::RigidBody2D>().get(block).bodyType = ecs::BodyType::Kinematic;
            registry.addComponent(block, TerrainBlock{.halfHeight = section.height / 2.0F});
            registry.addComponent(
                block,
                game::EntityNetState{.subtype = static_cast<std::uint8_t>(section.height / game::TERRAIN_HEIGHT_UNIT)});
            ecs::net::replicate(registry, block, game::KIND_OBSTACLE);
            section.lastBlock = block;
            --section.blocksLeft;
        }
    }
    std::erase_if(_sections, [](const Section& section) { return section.blocksLeft == 0; });
    registry.runSystem<TerrainBlock, ecs::RigidBody2D>(
        [velocity](ecs::Entity /*block*/, const TerrainBlock& /*tag*/, ecs::RigidBody2D& body) {
            body.velocityX = velocity;
        });
}

void TerrainPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_playedToken) {
        _bus->unsubscribe<LevelEventPlayed>(*_playedToken);
        _playedToken.reset();
    }
    if (_startedToken) {
        _bus->unsubscribe<LevelStarted>(*_startedToken);
        _startedToken.reset();
    }
}

}  // namespace rtype::server::gameplay
