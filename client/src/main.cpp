/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <exception>
#include <iostream>

#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"
#include "render/window/WindowConfig.hpp"
#include "systems/render/RenderSystem.hpp"

namespace {

constexpr float PROJECTILE_SPEED = 200.0F;
constexpr float SCENE_WIDTH = 1280.0F;

/** @brief Tag component marking the entities moved by `moveProjectiles`. */
struct Projectile {};

// Placeholder scene until entities come from the server: two projectiles cross an obstacle,
// one drawn behind it and one in front, to check depth layering by eye.
void spawnDemoScene(ecs::Registry& registry) {
    registry.addComponent<ecs::CameraComponent>(registry.spawnEntity(), ecs::CameraComponent{});

    const ecs::Entity obstacle = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(obstacle,
                                          ecs::Transform{.x = 640.0F, .y = 360.0F, .scaleX = 3.0F, .scaleY = 8.0F});
    registry.addComponent<ecs::Renderable>(
        obstacle, ecs::Renderable{.tint = {.r = 120, .g = 120, .b = 120, .a = 255}, .layer = 1});

    const ecs::Entity behind = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(behind, ecs::Transform{.x = 0.0F, .y = 330.0F, .scaleX = 2.0F});
    registry.addComponent<ecs::Renderable>(
        behind, ecs::Renderable{.tint = {.r = 80, .g = 160, .b = 255, .a = 255}, .layer = 0});
    registry.addComponent<Projectile>(behind, Projectile{});

    const ecs::Entity front = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(front, ecs::Transform{.x = 0.0F, .y = 390.0F, .scaleX = 2.0F});
    registry.addComponent<ecs::Renderable>(
        front, ecs::Renderable{.tint = {.r = 255, .g = 100, .b = 80, .a = 255}, .layer = 2});
    registry.addComponent<Projectile>(front, Projectile{});
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

        ecs::Registry registry;
        spawnDemoScene(registry);

        ecs::time::Clock clock;
        while (!render::Window::shouldClose()) {
            clock.tick(ecs::time::DEFAULT_FIXED_DT,
                       [&registry](const ecs::time::Time& time) { moveProjectiles(registry, time.deltaTime); });

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
