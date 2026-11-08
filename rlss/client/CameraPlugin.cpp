#include "CameraPlugin.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "Arena.hpp"
#include "ArenaViewPlugin.hpp"
#include "CarViewPlugin.hpp"
#include "ClientEvents.hpp"
#include "components/CameraComponent.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "protocol/Protocol.hpp"
#include "render/RenderPlugin.hpp"

namespace rlss::client {

namespace {

game::CameraRig* rigOf(ecs::Registry& registry, const ecs::Entity camera) {
    auto& rigs = registry.getComponents<game::CameraRig>();
    return rigs.contains(camera) ? &rigs.get(camera) : nullptr;
}

}  // namespace

void CameraPlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<ArenaViewPlugin>();
    builder.require<CarViewPlugin>();
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

void CameraPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    const auto& window = engine.service<engine::IWindowService>();
    const auto width = static_cast<float>(window.width());
    const auto height = static_cast<float>(window.height());

    _camera = registry.spawnEntity();
    registry.addComponent(_camera, ecs::CameraComponent{
                                       .targetX = (arena::LEFT + arena::RIGHT) / 2.0F,
                                       .targetY = (arena::TOP + arena::BOTTOM) / 2.0F,
                                       .offsetX = width / 2.0F,
                                       .offsetY = height / 2.0F,
                                   });
    registry.addComponent(_camera, ecs::Tag{.name = std::string(game::CAMERA_TAG)});
    // What is drawn around the play area: the walls, and the goals on the sides.
    registry.addComponent(_camera, game::CameraRig{
                                       .viewWidth = width,
                                       .viewHeight = height,
                                       .boundsLeft = arena::LEFT - arena::GOAL_DEPTH - ARENA_WALL,
                                       .boundsTop = arena::TOP - ARENA_WALL,
                                       .boundsRight = arena::RIGHT + arena::GOAL_DEPTH + ARENA_WALL,
                                       .boundsBottom = arena::BOTTOM + ARENA_WALL,
                                   });
    registry.addComponent(_camera, ecs::ScriptComponent{
                                       .moduleName = std::string(game::CAMERA_SCRIPT_MODULE),
                                       .className = "CameraDirector",
                                       .hasUpdate = true,
                                   });

    auto& events = engine.eventBus();
    _packets = events.subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
        const auto message = protocol::decode(packet.data);
        if (!message) {
            return;
        }
        if (const auto* welcome = std::get_if<protocol::Welcome>(&*message)) {
            _localCar = welcome->carId;
        } else if (const auto* status = std::get_if<protocol::MatchStatus>(&*message)) {
            if (auto* rig = rigOf(engine.registry(), _camera)) {
                rig->phase = static_cast<int>(status->phase);
            }
        }
    });
    _spawned = events.subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) {
            auto* rig = rigOf(engine.registry(), _camera);
            if (rig != nullptr && event.kind == game::KIND_BALL) {
                rig->ball = static_cast<std::int64_t>(event.entity);
            }
        });
    // A new match: its `Welcome` names the local car; the network id of the previous one means nothing anymore.
    _joined = events.subscribe<MatchJoined>([this](const MatchJoined& /*event*/) { _localCar.reset(); });
    _left = events.subscribe<MatchLeft>([this, &engine](const MatchLeft& /*event*/) {
        _localCar.reset();
        if (auto* rig = rigOf(engine.registry(), _camera)) {
            rig->phase = static_cast<int>(protocol::MatchPhase::Playing);
        }
    });
    _despawned = events.subscribe<ecs::net::ReplicatedEntityDespawned>(
        [this, &engine](const ecs::net::ReplicatedEntityDespawned& event) {
            auto* rig = rigOf(engine.registry(), _camera);
            if (rig != nullptr && std::cmp_equal(rig->ball, event.entity)) {
                rig->ball = game::NO_ENTITY;
            }
        });
}

void CameraPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    ecs::Registry& registry = engine.registry();
    auto* rig = rigOf(registry, _camera);
    if (rig == nullptr) {
        return;
    }
    rig->car = game::NO_ENTITY;
    if (_localCar && registry.hasResource<ecs::net::NetworkEntityMap>()) {
        if (const auto car = registry.getResource<ecs::net::NetworkEntityMap>().entityOf(*_localCar)) {
            rig->car = static_cast<std::int64_t>(*car);
        }
    }
}

void CameraPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_packets) {
        events.unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    if (_spawned) {
        events.unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_despawned) {
        events.unsubscribe<ecs::net::ReplicatedEntityDespawned>(*_despawned);
        _despawned.reset();
    }
    if (_joined) {
        events.unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        events.unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
}

}  // namespace rlss::client
