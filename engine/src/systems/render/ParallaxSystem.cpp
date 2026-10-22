/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Scrolls background entities tagged with ParallaxLayer.
*/

#include "systems/render/ParallaxSystem.hpp"

#include "components/CameraComponent.hpp"
#include "components/ParallaxLayer.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

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
            transform.x += (layer.scrollSpeedX * deltaTime) + (layer.cameraFactorX * cameraDeltaX);
            transform.y += (layer.scrollSpeedY * deltaTime) + (layer.cameraFactorY * cameraDeltaY);

            if (layer.wrapMinX != layer.wrapMaxX && transform.x < layer.wrapMinX) {
                transform.x = layer.wrapMaxX;
            }
            if (layer.wrapMaxX != layer.wrapMinX && transform.x > layer.wrapMaxX) {
                transform.x = layer.wrapMinX;
            }
            if (layer.wrapMinY != layer.wrapMaxY && transform.y < layer.wrapMinY) {
                transform.y = layer.wrapMaxY;
            }
            if (layer.wrapMaxY != layer.wrapMinY && transform.y > layer.wrapMaxY) {
                transform.y = layer.wrapMinY;
            }
        });
}

}  // namespace systems
