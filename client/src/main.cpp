/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <algorithm>
#include <exception>
#include <iostream>
#include <vector>

#include "components/CameraComponent.hpp"
#include "components/Collider2D.hpp"
#include "components/InputReceiver.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
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
#include "systems/render/RenderSystem.hpp"

namespace {

constexpr float PROJECTILE_SPEED = 200.0F;
constexpr float PLAYER_BULLET_SPEED = 700.0F;
constexpr float SCENE_WIDTH = 1280.0F;
constexpr float MIN_X = 32.0F;
constexpr float MAX_X = 1248.0F;
constexpr float MIN_Y = 32.0F;
constexpr float MAX_Y = 688.0F;

/** @brief Tag component marking scene obstacle projectiles. */
struct Projectile {};

/** @brief Tag component marking projectiles fired by the player. */
struct PlayerBullet {};

// Demo scene: moving projectiles, obstacle, and interactive player-controlled block.
void spawnDemoScene(ecs::Registry& registry) {
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});

    // Static obstacle
    const ecs::Entity obstacle = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(obstacle,
                                          ecs::Transform{.x = 640.0F, .y = 360.0F, .scaleX = 3.0F, .scaleY = 8.0F});
    registry.addComponent<ecs::Renderable>(
        obstacle, ecs::Renderable{.tint = {.r = 120, .g = 120, .b = 120, .a = 255}, .layer = 1});
    registry.addComponent<ecs::RigidBody2D>(obstacle, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(obstacle, ecs::Collider2D{.halfWidth = 48.0F, .halfHeight = 128.0F});

    // Projectile behind the obstacle
    const ecs::Entity behind = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(behind, ecs::Transform{.x = 0.0F, .y = 330.0F, .scaleX = 2.0F});
    registry.addComponent<ecs::Renderable>(
        behind, ecs::Renderable{.tint = {.r = 80, .g = 160, .b = 255, .a = 255}, .layer = 0});
    registry.addComponent<ecs::RigidBody2D>(
        behind, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = PROJECTILE_SPEED, .bullet = true});
    registry.addComponent<ecs::Collider2D>(behind,
                                           ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 16.0F});

    // Projectile in front of the obstacle
    const ecs::Entity front = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(front, ecs::Transform{.x = 0.0F, .y = 390.0F, .scaleX = 2.0F});
    registry.addComponent<ecs::Renderable>(
        front, ecs::Renderable{.tint = {.r = 255, .g = 100, .b = 80, .a = 255}, .layer = 2});
    registry.addComponent<Projectile>(front, Projectile{});

    // Controllable player block (green)
    const ecs::Entity player = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(player,
                                          ecs::Transform{.x = 200.0F, .y = 360.0F, .scaleX = 2.0F, .scaleY = 2.0F});
    registry.addComponent<ecs::Renderable>(player,
                                           ecs::Renderable{.tint = {.r = 50, .g = 220, .b = 90, .a = 255}, .layer = 3});
    registry.addComponent<ecs::InputReceiver>(player, ecs::InputReceiver{.playerIndex = 0, .speed = 350.0F});
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

void handlePlayerShoot(ecs::Registry& registry) {
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
            bullet, ecs::Transform{.x = pos.x + 40.0F, .y = pos.y, .scaleX = 1.0F, .scaleY = 0.4F});
        registry.addComponent<ecs::Renderable>(
            bullet, ecs::Renderable{.tint = {.r = 80, .g = 255, .b = 140, .a = 255}, .layer = 2});
        registry.addComponent<PlayerBullet>(bullet, PlayerBullet{});
    }
}

void movePlayerBullets(ecs::Registry& registry, float deltaTime) {
    std::vector<ecs::Entity> toDestroy;
    registry.runSystem<ecs::Transform, PlayerBullet>(
        [&toDestroy, deltaTime](ecs::Entity entity, ecs::Transform& transform, PlayerBullet& /*tag*/) {
            transform.x += PLAYER_BULLET_SPEED * deltaTime;
            if (transform.x > SCENE_WIDTH + 64.0F) {
                toDestroy.push_back(entity);
            }
        });
    for (const ecs::Entity entity : toDestroy) {
        registry.killEntity(entity);
    }
}

void moveProjectiles(ecs::Registry& registry, float deltaTime) {
    registry.runSystem<ecs::Transform, Projectile>(
        [deltaTime](ecs::Entity /*entity*/, ecs::Transform& transform, Projectile& /*tag*/) {
            transform.x += PROJECTILE_SPEED * deltaTime;
            if (transform.x > SCENE_WIDTH) {
                transform.x = 0.0F;
            }
        });
}

}  // namespace

int main() {
    try {
        const render::Window window(render::WindowConfig{});
        const render::TextureStore textures;
        render::RenderSystem renderSystem(textures);
        systems::InputSystem inputSystem;
        ecs::EventBus eventBus;

        ecs::Registry registry;
        systems::PhysicsSystem physicsSystem(registry);
        spawnDemoScene(registry);

        ecs::time::Clock clock;
        while (!render::Window::shouldClose()) {
            inputSystem.update(registry, &eventBus);
            handlePlayerShoot(registry);

            clock.tick(ecs::time::DEFAULT_FIXED_DT, [&](const ecs::time::Time& time) {
                movePlayer(registry, time.deltaTime);
                movePlayerBullets(registry, time.deltaTime);
                moveProjectiles(registry, time.deltaTime);
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
