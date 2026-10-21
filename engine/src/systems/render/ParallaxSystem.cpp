/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Scrolls background entities tagged with ParallaxLayer.
*/

#include "systems/render/ParallaxSystem.hpp"

#include "components/ParallaxLayer.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace systems {

void ParallaxSystem::update(ecs::Registry& registry, float deltaTime) {
    registry.runSystem<ecs::Transform, ecs::ParallaxLayer>(
        [deltaTime](ecs::Entity /*entity*/, ecs::Transform& transform, const ecs::ParallaxLayer& layer) {
            transform.x -= layer.scrollSpeedX * deltaTime;
            if (transform.x < layer.wrapMinX) {
                transform.x = layer.wrapMaxX;
            }
        });
}

}  // namespace systems
