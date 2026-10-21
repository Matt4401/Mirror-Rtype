/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <vector>

#include "components/AnimationComponent.hpp"
#include "components/CameraComponent.hpp"
#include "components/InputReceiver.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "input/InputAction.hpp"
#include "input/InputState.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/input/InputSystem.hpp"
#include "systems/physics/PhysicsSystem.hpp"
#include "systems/render/AnimationSystem.hpp"
#include "systems/render/ParallaxSystem.hpp"
#include "systems/render/RenderSystem.hpp"

namespace {

constexpr float PLAYER_BULLET_SPEED = 700.0F;
constexpr float SCENE_WIDTH = 1280.0F;
constexpr float SCENE_HEIGHT = 720.0F;
constexpr float MIN_X = 32.0F;
constexpr float MAX_X = 1248.0F;
constexpr float MIN_Y = 32.0F;
constexpr float MAX_Y = 688.0F;

struct PlayerBullet {};

struct StarLayer {
    int count = 0;
    float speed = 0.0F;
    ecs::Rgba color{};
    int layer = 0;
    ecs::TextureId texId = 0;
};

void spawnStarfield(ecs::Registry& registry, ecs::TextureId smallId, ecs::TextureId mediumId, ecs::TextureId largeId) {
    const std::array<StarLayer, 3> layers{{
        {.count = 80, .speed = 25.0F,  .color = {200, 200, 220, 80},  .layer = -3, .texId = smallId},
        {.count = 40, .speed = 60.0F,  .color = {220, 220, 255, 150}, .layer = -2, .texId = mediumId},
        {.count = 20, .speed = 120.0F, .color = {255, 255, 255, 220}, .layer = -1, .texId = largeId},
    }};

    int seed = 0;
    for (const auto& def : layers) {
        for (int i = 0; i < def.count; ++i) {
            const float x = static_cast<float>(((seed * 137) + 43) % static_cast<int>(SCENE_WIDTH));
            const float y = static_cast<float>(((seed * 97)  + 17) % static_cast<int>(SCENE_HEIGHT));
            ++seed;

            const ecs::Entity star = registry.spawnEntity();
            registry.addComponent<ecs::Transform>(star, ecs::Transform{.x = x, .y = y});
            registry.addComponent<ecs::Renderable>(
                star, ecs::Renderable{.textureId = def.texId, .tint = def.color, .layer = def.layer});
            registry.addComponent<ecs::ParallaxLayer>(
                star, ecs::ParallaxLayer{.scrollSpeedX = def.speed, .wrapMinX = -8.0F, .wrapMaxX = SCENE_WIDTH});
        }
    }
}

void spawnDemoScene(ecs::Registry& registry, ecs::TextureId shipTexId, ecs::TextureId bydosTexId) {
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});

    const ecs::Entity player = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(
        player, ecs::Transform{.x = 200.0F, .y = 360.0F, .scaleX = 2.0F, .scaleY = 2.0F});
    registry.addComponent<ecs::Renderable>(
        player, ecs::Renderable{.textureId = shipTexId,
                                .sourceRect = {.x = 0, .y = 0, .width = 33, .height = 17},
                                .origin = {.x = 0.5F, .y = 0.5F},
                                .tint = {255, 255, 255, 255},
                                .layer = 3,});
    registry.addComponent<ecs::AnimationComponent>(
        player, ecs::AnimationComponent{.frameWidth = 33,
                                         .frameHeight = 17,
                                         .frameCount = 5,
                                         .startX = 0,
                                         .startY = 0,
                                         .frameDuration = 0.12F,});
    registry.addComponent<ecs::InputReceiver>(player, ecs::InputReceiver{.playerIndex = 0, .speed = 350.0F});

    constexpr int BYDOS_FRAME_W = 33;
    constexpr int BYDOS_FRAME_H = 36;
    const ecs::Entity bydos = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(
        bydos, ecs::Transform{.x = 900.0F, .y = 300.0F, .scaleX = 2.0F, .scaleY = 2.0F});
    registry.addComponent<ecs::Renderable>(
        bydos, ecs::Renderable{.textureId = bydosTexId,
                               .sourceRect = {.x = 0, .y = 0, .width = BYDOS_FRAME_W, .height = BYDOS_FRAME_H},
                               .origin = {.x = 0.5F, .y = 0.5F},
                               .tint = {255, 255, 255, 255},
                               .layer = 2,});
    registry.addComponent<ecs::AnimationComponent>(
        bydos, ecs::AnimationComponent{.frameWidth = BYDOS_FRAME_W,
                                        .frameHeight = BYDOS_FRAME_H,
                                        .frameCount = 8,
                                        .startX = 0,
                                        .startY = 0,
                                        .frameDuration = 0.12F,});
}

void movePlayer(ecs::Registry& registry, float deltaTime) {
    registry.runSystem<ecs::Transform, ecs::InputReceiver>(
        [deltaTime](ecs::Entity /*entity*/, ecs::Transform& transform, ecs::InputReceiver& receiver) {
            if (!receiver.enabled) {
                return;
            }
            transform.x += receiver.moveX * receiver.speed * deltaTime;
            transform.y += receiver.moveY * receiver.speed * deltaTime;
            transform.x = std::clamp(transform.x, MIN_X, MAX_X);
            transform.y = std::clamp(transform.y, MIN_Y, MAX_Y);
        });
}

void handlePlayerShoot(ecs::Registry& registry, ecs::TextureId bulletTexId) {
    if (!registry.hasResource<input::InputState>()) {
        return;
    }
    const auto& inputState = registry.getResource<input::InputState>();
    if (!inputState.isJustPressed(input::Action::Shoot)) {
        return;
    }

    std::vector<ecs::Transform> spawnPositions;
    registry.runSystem<ecs::Transform, ecs::InputReceiver>(
        [&spawnPositions](ecs::Entity /*entity*/, ecs::Transform& transform, ecs::InputReceiver& receiver) {
            if (receiver.enabled) {
                spawnPositions.push_back(transform);
            }
        });

    for (const auto& pos : spawnPositions) {
        const ecs::Entity bullet = registry.spawnEntity();
        registry.addComponent<ecs::Transform>(
            bullet, ecs::Transform{.x = pos.x + 40.0F, .y = pos.y, .scaleX = 1.5F, .scaleY = 1.5F});
        registry.addComponent<ecs::Renderable>(
            bullet, ecs::Renderable{.textureId = bulletTexId,
                                    .sourceRect = {.x = 0, .y = 0, .width = 32, .height = 32},
                                    .origin = {.x = 0.5F, .y = 0.5F},
                                    .tint = {255, 255, 255, 255},
                                    .layer = 2});
        registry.addComponent<ecs::AnimationComponent>(
            bullet, ecs::AnimationComponent{.frameWidth = 32,
                                             .frameHeight = 32,
                                             .frameCount = 4,
                                             .startX = 0,
                                             .startY = 0,
                                             .frameDuration = 0.015F,
                                             .loop = false});
        registry.addComponent<PlayerBullet>(bullet, PlayerBullet{});
    }
}

void movePlayerBullets(ecs::Registry& registry, float deltaTime) {
    std::vector<ecs::Entity> toDestroy;
    registry.runSystem<ecs::Transform, PlayerBullet, ecs::AnimationComponent>(
        [&toDestroy, deltaTime](ecs::Entity entity, ecs::Transform& transform, PlayerBullet& /*tag*/, ecs::AnimationComponent& anim) {
            if (anim.currentFrame < anim.frameCount - 1) {
                return;
            }
            transform.x += PLAYER_BULLET_SPEED * deltaTime;
            if (transform.x > SCENE_WIDTH + 64.0F) {
                toDestroy.push_back(entity);
            }
        });
    for (const ecs::Entity entity : toDestroy) {
        registry.killEntity(entity);
    }
}

}  // namespace

int main() {
    try {
        const render::Window window(render::WindowConfig{});
        render::TextureStore textures;

        const ecs::TextureId shipTexId    = textures.load("sprites/r-typesheet42.gif");
        const ecs::TextureId bydosTexId   = textures.load("sprites/spritesheet5.png");
        const ecs::TextureId bulletTexId  = textures.load("sprites/r-typesheet-amo.png");
        const ecs::TextureId starSmallId  = textures.load("sprites/star_small.png");
        const ecs::TextureId starMediumId = textures.load("sprites/star_medium.png");
        const ecs::TextureId starLargeId  = textures.load("sprites/star_large.png");

        render::RenderSystem  renderSystem(textures);
        systems::InputSystem  inputSystem;
        systems::ParallaxSystem parallaxSystem;
        systems::AnimationSystem animationSystem;
        ecs::EventBus eventBus;

        ecs::Registry registry;
        systems::PhysicsSystem physicsSystem(registry);

        spawnStarfield(registry, starSmallId, starMediumId, starLargeId);
        spawnDemoScene(registry, shipTexId, bydosTexId);

        ecs::time::Clock clock;
        while (!render::Window::shouldClose()) {
            inputSystem.update(registry, &eventBus);
            handlePlayerShoot(registry, bulletTexId);

            clock.tick(ecs::time::DEFAULT_FIXED_DT, [&](const ecs::time::Time& time) {
                parallaxSystem.update(registry, time.deltaTime);
                animationSystem.update(registry, time.deltaTime);
                movePlayer(registry, time.deltaTime);
                movePlayerBullets(registry, time.deltaTime);
                physicsSystem.update(time.deltaTime);
            });

            render::Window::beginFrame();
            renderSystem.update(registry);
            render::Window::endFrame();
        }
    } catch (const std::exception& error) {
        std::cerr << "client: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
