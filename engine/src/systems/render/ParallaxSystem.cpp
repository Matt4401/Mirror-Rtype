/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Scrolls background entities tagged with ParallaxLayer.
*/

#include "systems/render/ParallaxSystem.hpp"

#include "components/CameraComponent.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/SpriteInstances.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace {

void applyWrap(float& value, float wrapMin, float wrapMax) {
    if (wrapMin == wrapMax) {
        return;
    }
    if (value < wrapMin) {
        value = wrapMax;
    } else if (value > wrapMax) {
        value = wrapMin;
    }
}

void scroll(float& x, float& y, const ecs::ParallaxLayer& layer, float dx, float dy) {
    x += dx;
    y += dy;
    applyWrap(x, layer.wrapMinX, layer.wrapMaxX);
    applyWrap(y, layer.wrapMinY, layer.wrapMaxY);
}

}  // namespace

namespace systems {

void ParallaxSystem::update(ecs::Registry& registry, float deltaTime) {
    float cameraX = _prevCameraX;
    float cameraY = _prevCameraY;

    registry.runSystem<ecs::CameraComponent>(
        [&cameraX, &cameraY](ecs::Entity /*entity*/, const ecs::CameraComponent& cam) {
            if (cam.active) {
                cameraX = cam.targetX;
                cameraY = cam.targetY;
            }
        });

    const float cameraDeltaX = cameraX - _prevCameraX;
    const float cameraDeltaY = cameraY - _prevCameraY;
    _prevCameraX = cameraX;
    _prevCameraY = cameraY;

    registry.runSystem<ecs::Transform, ecs::ParallaxLayer>(
        [deltaTime, cameraDeltaX, cameraDeltaY](ecs::Entity /*entity*/, ecs::Transform& transform,
                                                const ecs::ParallaxLayer& layer) {
            const float dx = (layer.scrollSpeedX * deltaTime) + (layer.cameraFactorX * cameraDeltaX);
            const float dy = (layer.scrollSpeedY * deltaTime) + (layer.cameraFactorY * cameraDeltaY);
            scroll(transform.x, transform.y, layer, dx, dy);
        });

    registry.runSystem<ecs::SpriteInstances, ecs::ParallaxLayer>(
        [deltaTime, cameraDeltaX, cameraDeltaY](ecs::Entity /*entity*/, ecs::SpriteInstances& pool,
                                                const ecs::ParallaxLayer& layer) {
            const float dx = (layer.scrollSpeedX * deltaTime) + (layer.cameraFactorX * cameraDeltaX);
            const float dy = (layer.scrollSpeedY * deltaTime) + (layer.cameraFactorY * cameraDeltaY);
            for (auto& inst : pool.instances) {
                scroll(inst.x, inst.y, layer, dx, dy);
            }
        });
}

}  // namespace systems
