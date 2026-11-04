/**
 * @file RTypeGamePlugin.cpp
 * @brief Implementation of RTypeGamePlugin demonstration game logic.
 */

#include "RTypeGamePlugin.hpp"

#include <algorithm>
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
#include "components/Renderable.hpp"
#include "components/ShipInput.hpp"
#include "components/SpriteInstances.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IInputService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "prefabs/ClientBullet.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "render/RenderPlugin.hpp"
#include "render/texture/TextureStore.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"

namespace client {

namespace {

constexpr float SCENE_WIDTH = 1280.0F;
constexpr float SCENE_HEIGHT = 720.0F;
constexpr float MIN_X = 32.0F;
constexpr float MAX_X = 1248.0F;
constexpr float MIN_Y = 32.0F;
constexpr float MAX_Y = 688.0F;
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

void spawnDemoScene(ecs::Registry& registry, ecs::TextureId shipTexId, ecs::TextureId bydosTexId) {
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});

    const ecs::Entity player = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(player,
                                          ecs::Transform{.x = 200.0F, .y = 360.0F, .scaleX = 2.0F, .scaleY = 2.0F});
    registry.addComponent<ecs::Renderable>(player, ecs::Renderable{
                                                       .textureId = shipTexId,
                                                       .sourceRect = {.x = 0, .y = 0, .width = 33, .height = 17},
                                                       .origin = {.x = 0.5F, .y = 0.5F},
                                                       .tint = COLOR_WHITE,
                                                       .layer = 3,
                                                   });
    registry.addComponent<ecs::AnimationComponent>(player, ecs::AnimationComponent{
                                                               .frameWidth = 33,
                                                               .frameHeight = 17,
                                                               .frameCount = 5,
                                                               .startX = 0,
                                                               .startY = 0,
                                                               .frameDuration = 0.12F,
                                                           });
    registry.addComponent<client::ShipInput>(player, client::ShipInput{.playerIndex = 0, .speed = 350.0F});
    registry.addComponent<InputState>(player, InputState{.playerIndex = 0, .actions = {}});

    constexpr int BYDOS_FRAME_W = 33;
    constexpr int BYDOS_FRAME_H = 36;
    const ecs::Entity bydos = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(bydos,
                                          ecs::Transform{.x = 900.0F, .y = 300.0F, .scaleX = 2.0F, .scaleY = 2.0F});
    registry.addComponent<ecs::Renderable>(
        bydos, ecs::Renderable{
                   .textureId = bydosTexId,
                   .sourceRect = {.x = 0, .y = 0, .width = BYDOS_FRAME_W, .height = BYDOS_FRAME_H},
                   .origin = {.x = 0.5F, .y = 0.5F},
                   .tint = COLOR_WHITE,
                   .layer = 2,
               });
    registry.addComponent<ecs::AnimationComponent>(bydos, ecs::AnimationComponent{
                                                              .frameWidth = BYDOS_FRAME_W,
                                                              .frameHeight = BYDOS_FRAME_H,
                                                              .frameCount = 8,
                                                              .startX = 0,
                                                              .startY = 0,
                                                              .frameDuration = 0.12F,
                                                          });
}

void movePlayer(ecs::Registry& registry, float deltaTime) {
    registry.runSystem<ecs::Transform, client::ShipInput, InputState>(
        [deltaTime](ecs::Entity /*entity*/, ecs::Transform& transform, client::ShipInput& ship,
                    const InputState& inputState) {
            if (!ship.enabled) {
                return;
            }
            const auto move = inputState.getVector2("move_x", "move_y", true);
            transform.x += move.x * ship.speed * deltaTime;
            transform.y += move.y * ship.speed * deltaTime;
            transform.x = std::clamp(transform.x, MIN_X, MAX_X);
            transform.y = std::clamp(transform.y, MIN_Y, MAX_Y);
        });
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

    spawnStarfield(engine.registry(), _starSmallId, _starMediumId, _starLargeId);
    spawnDemoScene(engine.registry(), _shipTexId, _bydosTexId);
}

void RTypeGamePlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    movePlayer(engine.registry(), time.deltaTime);
    killOffscreenBullets(engine.registry());
}

void RTypeGamePlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    std::vector<ecs::Transform> spawnPositions;
    engine.registry().runSystem<ecs::Transform, client::ShipInput, InputState>(
        [&spawnPositions](ecs::Entity /*entity*/, ecs::Transform& transform, client::ShipInput& ship,
                          const InputState& inputState) {
            if (ship.enabled && inputState.isJustPressed("shoot")) {
                spawnPositions.push_back(transform);
            }
        });

    const auto& prefabManager = engine.service<::prefabs::PrefabManager>();
    for (const auto& pos : spawnPositions) {
        prefabManager.spawn(client::prefabs::CLIENT_BULLET_PREFAB, _commandBuffer,
                            {{"x", pos.x + 40.0F}, {"y", pos.y}});
    }
    _commandBuffer.flush(engine.registry());
}

}  // namespace client
