/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <exception>
#include <iostream>

#include "components/Collider2D.hpp"
#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/physics/PhysicsSystem.hpp"
#include "systems/render/RenderSystem.hpp"

namespace {

constexpr float PROJECTILE_SPEED = 200.0F;

// Placeholder scene until entities come from the server: projectiles collide with a static obstacle.
void spawnDemoScene(ecs::Registry& registry) {
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});

    const ecs::Entity obstacle = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(obstacle,
                                          ecs::Transform{.x = 640.0F, .y = 360.0F, .scaleX = 3.0F, .scaleY = 8.0F});
    registry.addComponent<ecs::Renderable>(
        obstacle, ecs::Renderable{.tint = {.r = 120, .g = 120, .b = 120, .a = 255}, .layer = 1});
    registry.addComponent<ecs::RigidBody2D>(obstacle, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(obstacle, ecs::Collider2D{.halfWidth = 48.0F, .halfHeight = 128.0F});

    const ecs::Entity behind = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(behind, ecs::Transform{.x = 0.0F, .y = 330.0F, .scaleX = 2.0F});
    registry.addComponent<ecs::Renderable>(
        behind, ecs::Renderable{.tint = {.r = 80, .g = 160, .b = 255, .a = 255}, .layer = 0});
    registry.addComponent<ecs::RigidBody2D>(
        behind, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = PROJECTILE_SPEED, .bullet = true});
    registry.addComponent<ecs::Collider2D>(behind,
                                           ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 16.0F});

    const ecs::Entity front = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(front, ecs::Transform{.x = 0.0F, .y = 390.0F, .scaleX = 2.0F});
    registry.addComponent<ecs::Renderable>(
        front, ecs::Renderable{.tint = {.r = 255, .g = 100, .b = 80, .a = 255}, .layer = 2});
    registry.addComponent<ecs::RigidBody2D>(
        front, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = PROJECTILE_SPEED, .bullet = true});
    registry.addComponent<ecs::Collider2D>(front,
                                           ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 16.0F});
}

}  // namespace

int main() {
    try {
        const render::Window window(render::WindowConfig{});
        const render::TextureStore textures;
        render::RenderSystem renderSystem(textures);

        ecs::Registry registry;
        systems::PhysicsSystem physicsSystem(registry);
        spawnDemoScene(registry);

        ecs::time::Clock clock;
        while (!render::Window::shouldClose()) {
            clock.tick(ecs::time::DEFAULT_FIXED_DT,
                       [&physicsSystem](const ecs::time::Time& time) { physicsSystem.update(time.deltaTime); });

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
