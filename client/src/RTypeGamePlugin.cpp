/**
 * @file RTypeGamePlugin.cpp
 * @brief Implementation of RTypeGamePlugin demonstration game logic.
 */

#include "RTypeGamePlugin.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "components/AnimationComponent.hpp"
#include "components/CameraComponent.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/Renderable.hpp"
#include "components/SpriteInstances.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IInputService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "net/ReplicationEvents.hpp"
#include "render/RenderPlugin.hpp"
#include "render/texture/TextureStore.hpp"
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
constexpr int BULLET_FRAME_SIZE = 32;
constexpr int BULLET_FRAME_COUNT = 4;
constexpr float BULLET_SCALE = 1.5F;
constexpr float BULLET_FRAME_DURATION = 0.015F;
constexpr float DEFAULT_FRAME_DURATION = 0.12F;
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

/** @brief Sprite of a replicated entity: one frame strip of a spritesheet. */
struct EntitySprite {
    ecs::TextureId texture = ecs::NO_TEXTURE;
    int frameWidth = 0;
    int frameHeight = 0;
    int frameCount = 1;
    int startY = 0;
    int layer = 0;
    float scale = SPRITE_SCALE;
    float frameDuration = DEFAULT_FRAME_DURATION;
    bool loop = true;
};

/** @brief Gives a replicated entity its sprite and its size on screen. */
void applySprite(ecs::Registry& registry, const ecs::Entity entity, const EntitySprite& sprite) {
    auto& transform = registry.getComponents<ecs::Transform>().get(entity);
    transform.scaleX = sprite.scale;
    transform.scaleY = sprite.scale;
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
                                                               .frameDuration = sprite.frameDuration,
                                                               .loop = sprite.loop,
                                                           });
}

}  // namespace

void RTypeGamePlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<::input::InputPlugin>();
    builder.require<systems::AnimationPlugin>();
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

    _starSmallId = load("sprites/star_small.png");
    _starMediumId = load("sprites/star_medium.png");
    _starLargeId = load("sprites/star_large.png");

    if (engine.mediator().hasService<engine::IInputService>()) {
        client::input::setupDefaultBindings(engine.service<engine::IInputService>().mapping());
    }

    ecs::Registry& registry = engine.registry();
    spawnStarfield(registry, _starSmallId, _starMediumId, _starLargeId);
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});
    // The local player's actions, sent to the server by the network plugins. Nothing of the game is spawned here:
    // ships, enemies and bullets come from the snapshots of the server.
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
    } else if (event.kind == rtype::game::KIND_ALIEN_SCOUT) {
        applySprite(registry, event.entity,
                    {
                        .texture = _bydosTexId,
                        .frameWidth = BYDOS_FRAME_WIDTH,
                        .frameHeight = BYDOS_FRAME_HEIGHT,
                        .frameCount = BYDOS_FRAME_COUNT,
                        .startY = 0,
                        .layer = 2,
                    });
    } else if (event.kind == rtype::game::KIND_PLAYER_BULLET) {
        applySprite(registry, event.entity,
                    {
                        .texture = _bulletTexId,
                        .frameWidth = BULLET_FRAME_SIZE,
                        .frameHeight = BULLET_FRAME_SIZE,
                        .frameCount = BULLET_FRAME_COUNT,
                        .startY = 0,
                        .layer = 2,
                        .scale = BULLET_SCALE,
                        .frameDuration = BULLET_FRAME_DURATION,
                        .loop = false,
                    });
    }
}

void RTypeGamePlugin::shutdown(engine::Engine& engine) {
    if (_spawned) {
        engine.eventBus().unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
}

}  // namespace client
