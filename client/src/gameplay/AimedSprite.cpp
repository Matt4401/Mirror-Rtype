#include "gameplay/AimedSprite.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"

namespace client::gameplay {

namespace {

/** @brief Degrees in a full turn. */
constexpr float FULL_TURN = 360.0F;

}  // namespace

int aimedFrame(const AimedSprite& sprite, const float degrees) noexcept {
    if (sprite.frames <= 1) {
        return 0;
    }
    const float step = FULL_TURN / static_cast<float>(sprite.frames);
    const float relative = std::fmod(std::fmod(degrees - sprite.firstAngle, FULL_TURN) + FULL_TURN, FULL_TURN);
    return static_cast<int>(std::lround(relative / step)) % sprite.frames;
}

void aimSprites(ecs::Registry& registry) {
    std::vector<std::pair<float, float>> ships;
    registry.runSystem<ecs::Replicated, ecs::Transform>(
        [&ships](ecs::Entity /*entity*/, const ecs::Replicated& replicated, const ecs::Transform& transform) {
            if (replicated.kind == rtype::game::KIND_PLAYER_SHIP) {
                ships.emplace_back(transform.x, transform.y);
            }
        });
    if (ships.empty()) {
        return;
    }
    registry.runSystem<AimedSprite, ecs::Transform, ecs::Renderable>([&ships](ecs::Entity /*entity*/,
                                                                              const AimedSprite& sprite,
                                                                              const ecs::Transform& transform,
                                                                              ecs::Renderable& renderable) {
        std::optional<std::pair<float, float>> target;
        float best = std::numeric_limits<float>::infinity();
        for (const auto& [x, y] : ships) {
            const float distance = ((x - transform.x) * (x - transform.x)) + ((y - transform.y) * (y - transform.y));
            if (distance < best) {
                best = distance;
                target = {x, y};
            }
        }
        const float degrees =
            std::atan2(target->second - transform.y, target->first - transform.x) * 180.0F / std::numbers::pi_v<float>;
        const int frame = aimedFrame(sprite, degrees);
        const int stride = sprite.frameStride > 0 ? sprite.frameStride : sprite.frameWidth;
        renderable.sourceRect = {.x = sprite.startX + (frame * stride),
                                 .y = sprite.startY,
                                 .width = sprite.frameWidth,
                                 .height = sprite.frameHeight};
    });
}

}  // namespace client::gameplay
