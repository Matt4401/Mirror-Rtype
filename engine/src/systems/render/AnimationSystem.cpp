/*
** EPITECH PROJECT, 2026
** Squacaron
** File description:
** Advances spritesheet animations and writes the current frame into Renderable.
*/

#include "systems/render/AnimationSystem.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>

#include "components/AnimationComponent.hpp"
#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace systems {

void AnimationSystem::update(ecs::Registry& registry, float deltaTime) {
    registry.runSystem<ecs::AnimationComponent, ecs::Renderable>(
        [deltaTime](ecs::Entity /*entity*/, ecs::AnimationComponent& anim, ecs::Renderable& renderable) {
            const bool varying = !anim.frameWidths.empty();
            const int count = varying ? static_cast<int>(anim.frameWidths.size()) : anim.frameCount;
            if (count <= 0) {
                return;
            }
            anim.elapsed += deltaTime;
            if (anim.elapsed >= anim.frameDuration) {
                anim.elapsed -= anim.frameDuration;
                if (anim.loop) {
                    anim.currentFrame = (anim.currentFrame + 1) % count;
                } else if (anim.currentFrame < count - 1) {
                    ++anim.currentFrame;
                }
            }
            anim.currentFrame = std::clamp(anim.currentFrame, 0, count - 1);
            int x = anim.startX;
            int width = anim.frameWidth;
            if (varying) {
                // Frames side by side: this one starts where the previous ones end.
                const auto current = static_cast<std::ptrdiff_t>(anim.currentFrame);
                x += std::accumulate(anim.frameWidths.begin(), anim.frameWidths.begin() + current, 0);
                width = anim.frameWidths[static_cast<std::size_t>(anim.currentFrame)];
            } else {
                x += anim.currentFrame * (anim.frameStride > 0 ? anim.frameStride : anim.frameWidth);
            }
            renderable.sourceRect = {
                .x = x,
                .y = anim.startY,
                .width = width,
                .height = anim.frameHeight,
            };
        });
}

}  // namespace systems
