/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Advances spritesheet animations and writes the current frame into Renderable.
*/

#include "systems/render/AnimationSystem.hpp"

#include "components/AnimationComponent.hpp"
#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"

namespace systems {

void AnimationSystem::update(ecs::Registry& registry, float deltaTime) {
    registry.runSystem<ecs::AnimationComponent, ecs::Renderable>(
        [deltaTime](ecs::Entity /*entity*/, ecs::AnimationComponent& anim, ecs::Renderable& renderable) {
            anim.elapsed += deltaTime;
            if (anim.elapsed >= anim.frameDuration) {
                anim.elapsed -= anim.frameDuration;
                if (anim.loop) {
                    anim.currentFrame = (anim.currentFrame + 1) % anim.frameCount;
                } else if (anim.currentFrame < anim.frameCount - 1) {
                    ++anim.currentFrame;
                }
            }
            renderable.sourceRect = {
                .x = anim.startX + (anim.currentFrame * anim.frameWidth),
                .y = anim.startY,
                .width = anim.frameWidth,
                .height = anim.frameHeight,
            };
        });
}

}  // namespace systems
