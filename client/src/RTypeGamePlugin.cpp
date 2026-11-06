/**
 * @file RTypeGamePlugin.cpp
 * @brief Implementation of RTypeGamePlugin demonstration game logic.
 */

#include "RTypeGamePlugin.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "components/AnimationComponent.hpp"
#include "components/CameraComponent.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/Predicted.hpp"
#include "components/Renderable.hpp"
#include "components/SpriteInstances.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IInputService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "net/ReplicationEvents.hpp"
#include "prefabs/ClientBullet.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "protocol/Messages.hpp"
#include "render/RenderPlugin.hpp"
#include "render/texture/TextureStore.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"

namespace client {

namespace {

constexpr float SCENE_WIDTH = 1280.0F;
constexpr float SCENE_HEIGHT = 720.0F;
constexpr float SPRITE_SCALE = 2.0F;
constexpr int SHIP_FRAME_WIDTH = 33;
constexpr int SHIP_FRAME_HEIGHT = 17;
constexpr int SHIP_FRAME_COUNT = 5;
constexpr int BYDOS_FRAME_WIDTH = 33;
constexpr int BYDOS_FRAME_HEIGHT = 36;
constexpr int BYDOS_FRAME_COUNT = 8;
constexpr auto KIND_ALIEN_SCOUT = rtype::protocol::makeEntityKind("alien_scout");
constexpr ecs::Rgba COLOR_WHITE = {.r = 255, .g = 255, .b = 255, .a = 255};
constexpr ecs::Rgba COLOR_STAR_SMALL = {.r = 200, .g = 200, .b = 220, .a = 80};
constexpr ecs::Rgba COLOR_STAR_MEDIUM = {.r = 220, .g = 220, .b = 255, .a = 150};
constexpr ecs::Rgba COLOR_STAR_LARGE = {.r = 255, .g = 255, .b = 255, .a = 220};

using ::input::InputState;

struct StarLayer {
    int count = 0;
    float speed = 0.0F;
    ecs::Rgba color{};
    int layer = 0;
    ecs::TextureId texId = ecs::NO_TEXTURE;
};

void spawnStarfield(ecs::Registry& registry, ecs::TextureId smallId, ecs::TextureId mediumId, ecs::TextureId largeId) {
    const std::array<StarLayer, 3> layers{
        {
            {.count = 80, .speed = 25.0F, .color = COLOR_STAR_SMALL, .layer = -3, .texId = smallId},
            {.count = 40, .speed = 60.0F, .color = COLOR_STAR_MEDIUM, .layer = -2, .texId = mediumId},
            {.count = 20, .speed = 120.0F, .color = COLOR_STAR_LARGE, .layer = -1, .texId = largeId},
        },
    };

    int seed = 0;
    for (const auto& def : layers) {
        ecs::SpriteInstances pool;
        pool.instances.reserve(static_cast<std::size_t>(def.count));
        for (int i = 0; i < def.count; ++i) {
            const auto x = static_cast<float>(((seed * 137) + 43) % static_cast<int>(SCENE_WIDTH));
            const auto y = static_cast<float>(((seed * 97) + 17) % static_cast<int>(SCENE_HEIGHT));
            ++seed;
            pool.instances.push_back({.x = x, .y = y});
        }
        const ecs::Entity star = registry.spawnEntity();
        registry.addComponent<ecs::SpriteInstances>(star, std::move(pool));
        registry.addComponent<ecs::Renderable>(
            star, ecs::Renderable{.textureId = def.texId, .tint = def.color, .layer = def.layer});
        registry.addComponent<ecs::ParallaxLayer>(star, ecs::ParallaxLayer{
                                                            .scrollSpeedX = -def.speed,
                                                            .wrapMinX = -8.0F,
                                                            .wrapMaxX = SCENE_WIDTH,
                                                        });
    }
}

void killOffscreenBullets(ecs::Registry& registry) {
    std::vector<ecs::Entity> toDestroy;
    registry.runSystem<ecs::Transform, client::prefabs::ClientBullet>(
        [&toDestroy](ecs::Entity entity, const ecs::Transform& transform, client::prefabs::ClientBullet& /*tag*/) {
            if (transform.x > SCENE_WIDTH + 64.0F) {
                toDestroy.push_back(entity);
            }
        });
    for (const ecs::Entity entity : toDestroy) {
        registry.killEntity(entity);
    }
}

/** @brief Sprite of a replicated entity: one frame strip of a spritesheet. */
struct EntitySprite {
    ecs::TextureId texture = ecs::NO_TEXTURE;
    int frameWidth = 0;
    int frameHeight = 0;
    int frameCount = 1;
    int startY = 0;
    int layer = 0;
};

/** @brief Gives a replicated entity its sprite and its size on screen. */
void applySprite(ecs::Registry& registry, const ecs::Entity entity, const EntitySprite& sprite) {
    auto& transform = registry.getComponents<ecs::Transform>().get(entity);
    transform.scaleX = SPRITE_SCALE;
    transform.scaleY = SPRITE_SCALE;
    registry.addComponent<ecs::Renderable>(
        entity,
        ecs::Renderable{
            .textureId = sprite.texture,
            .sourceRect = {.x = 0, .y = sprite.startY, .width = sprite.frameWidth, .height = sprite.frameHeight},
            .origin = {.x = 0.5F, .y = 0.5F},
            .tint = COLOR_WHITE,
            .layer = sprite.layer,
        });
    registry.addComponent<ecs::AnimationComponent>(entity, ecs::AnimationComponent{
                                                               .frameWidth = sprite.frameWidth,
                                                               .frameHeight = sprite.frameHeight,
                                                               .frameCount = sprite.frameCount,
                                                               .startX = 0,
                                                               .startY = sprite.startY,
                                                               .frameDuration = 0.12F,
                                                           });
}

}  // namespace

void RTypeGamePlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<::input::InputPlugin>();
    builder.require<systems::PhysicsPlugin>();
    builder.require<systems::AnimationPlugin>();
    builder.require<::prefabs::PrefabPlugin>();
}

void RTypeGamePlugin::init(engine::Engine& engine) {
    render::TextureStore* textures = engine.getPlugin<render::RenderPlugin>().textures();
    if (textures == nullptr) {
        throw std::runtime_error("RTypeGamePlugin: TextureStore not available");
    }

    const auto load = [&](const std::string& path) {
        const ecs::TextureId id = textures->load(path);
        if (id == ecs::NO_TEXTURE) {
            throw std::runtime_error("failed to load texture '" + path +
                                     "' — run the client from the project root directory");
        }
        return id;
    };

    _shipTexId = load("sprites/r-typesheet42.gif");
    _bydosTexId = load("sprites/spritesheet5.png");
    _bulletTexId = load("sprites/r-typesheet-amo.png");

    engine.getPlugin<::prefabs::PrefabPlugin>().manager().registerPrefab(
        client::prefabs::CLIENT_BULLET_PREFAB, client::prefabs::buildClientBullet,
        {{"textureId", static_cast<std::int64_t>(_bulletTexId)}, {"scaleX", 1.5F}, {"scaleY", 1.5F}});

    _starSmallId = load("sprites/star_small.png");
    _starMediumId = load("sprites/star_medium.png");
    _starLargeId = load("sprites/star_large.png");

    if (engine.mediator().hasService<engine::IInputService>()) {
        client::input::setupDefaultBindings(engine.service<engine::IInputService>().mapping());
    }

    ecs::Registry& registry = engine.registry();
    spawnStarfield(registry, _starSmallId, _starMediumId, _starLargeId);
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});
    // The local player's actions, read here for the bullets and by the network plugins for the server. The ship is not
    // spawned here: it comes from the snapshots of the server, like every other entity of the game.
    registry.addComponent<InputState>(registry.spawnEntity(), InputState{.playerIndex = 0, .actions = {}});

    _spawned = engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) { onEntitySpawned(engine, event); });
}

void RTypeGamePlugin::onEntitySpawned(engine::Engine& engine, const ecs::net::ReplicatedEntitySpawned& event) const {
    ecs::Registry& registry = engine.registry();
    if (event.kind == rtype::game::KIND_PLAYER_SHIP) {
        // Each row of the spritesheet is one ship color, so that players can tell their ships apart.
        const int row = registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype;
        applySprite(registry, event.entity,
                    {
                        .texture = _shipTexId,
                        .frameWidth = SHIP_FRAME_WIDTH,
                        .frameHeight = SHIP_FRAME_HEIGHT,
                        .frameCount = SHIP_FRAME_COUNT,
                        .startY = row * SHIP_FRAME_HEIGHT,
                        .layer = 3,
                    });
    } else if (event.kind == KIND_ALIEN_SCOUT) {
        applySprite(registry, event.entity,
                    {
                        .texture = _bydosTexId,
                        .frameWidth = BYDOS_FRAME_WIDTH,
                        .frameHeight = BYDOS_FRAME_HEIGHT,
                        .frameCount = BYDOS_FRAME_COUNT,
                        .startY = 0,
                        .layer = 2,
                    });
    }
}

void RTypeGamePlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    killOffscreenBullets(engine.registry());
}

void RTypeGamePlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    ecs::Registry& registry = engine.registry();
    bool shooting = false;
    registry.runSystem<InputState>([&shooting](ecs::Entity /*entity*/, const InputState& state) {
        shooting = shooting || state.isJustPressed("shoot");
    });
    if (shooting) {
        // TODO(ecs): the server does not simulate shots yet: these bullets only exist on this client.
        std::vector<ecs::Transform> spawnPositions;
        registry.runSystem<ecs::Predicted, ecs::Transform>(
            [&spawnPositions](ecs::Entity /*entity*/, const ecs::Predicted& /*local*/,
                              const ecs::Transform& transform) { spawnPositions.push_back(transform); });
        const auto& prefabManager = engine.service<::prefabs::PrefabManager>();
        for (const auto& pos : spawnPositions) {
            prefabManager.spawn(client::prefabs::CLIENT_BULLET_PREFAB, _commandBuffer,
                                {{"x", pos.x + 40.0F}, {"y", pos.y}});
        }
    }
    _commandBuffer.flush(registry);
}

void RTypeGamePlugin::shutdown(engine::Engine& engine) {
    if (_spawned) {
        engine.eventBus().unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
}

}  // namespace client
