#include "gameplay/BossEffects.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"

namespace client::gameplay {

namespace {

/** @brief Offsets of the bursts after the first one, as fractions of the half-size of the boss: a fixed scatter. */
constexpr std::array<std::array<float, 2>, EXPLOSION_BURSTS - 1> BURST_OFFSETS{{
    {{-0.6F, -0.4F}},
    {{0.5F, 0.5F}},
    {{0.3F, -0.7F}},
    {{-0.4F, 0.6F}},
    {{0.8F, -0.1F}},
    {{-0.8F, 0.1F}},
    {{0.1F, 0.8F}},
    {{-0.1F, -0.9F}},
}};
/** @brief Part of its hit points at spawn under which an entity that disappears is taken as killed. */
constexpr int KILLED_FRACTION = 10;

}  // namespace

void updateHitFlashes(ecs::Registry& registry, const float deltaTime) {
    registry.runSystem<HitFlash, rtype::game::EntityNetState, ecs::Renderable>(
        [deltaTime](ecs::Entity /*entity*/, HitFlash& flash, const rtype::game::EntityNetState& state,
                    ecs::Renderable& renderable) {
            if (state.health < flash.lastHealth) {
                flash.remaining = HIT_FLASH_DURATION;
            } else {
                flash.remaining = std::max(0.0F, flash.remaining - deltaTime);
            }
            flash.lastHealth = state.health;
            renderable.tint = flash.remaining > 0.0F ? HIT_FLASH_TINT : flash.baseTint;
        });
}

bool diedInCombat(const HitFlash& flash) noexcept {
    return flash.lastHealth <= 1 || static_cast<int>(flash.lastHealth) * KILLED_FRACTION <= flash.spawnHealth;
}

std::vector<BurstSpec> explosionBursts(const float x, const float y, const float width, const float height) {
    std::vector<BurstSpec> bursts;
    bursts.reserve(EXPLOSION_BURSTS);
    bursts.push_back({.x = x, .y = y, .delay = 0.0F});
    const float step = EXPLOSION_SPREAD / static_cast<float>(EXPLOSION_BURSTS - 1);
    float delay = 0.0F;
    for (const auto& [dx, dy] : BURST_OFFSETS) {
        delay += step;
        bursts.push_back({.x = x + (dx * width / 2.0F), .y = y + (dy * height / 2.0F), .delay = delay});
    }
    return bursts;
}

std::optional<BossHealth> bossHealth(const ecs::Registry& registry) {
    std::optional<BossHealth> total;
    const auto* flashes = registry.findComponents<HitFlash>();
    if (flashes == nullptr) {
        return total;
    }
    for (const ecs::Entity entity : flashes->getDenseEntities()) {
        if (!registry.hasComponent<ecs::Replicated>(entity) ||
            registry.getComponents<ecs::Replicated>().get(entity).kind != rtype::game::KIND_BOSS_CORE ||
            !registry.hasComponent<rtype::game::EntityNetState>(entity)) {
            continue;
        }
        BossHealth& sum = total ? *total : total.emplace();
        sum.points += static_cast<float>(registry.getComponents<rtype::game::EntityNetState>().get(entity).health);
        sum.max += static_cast<float>(flashes->get(entity).spawnHealth);
    }
    return total;
}

void updateExplosions(ecs::Registry& registry, const float deltaTime) {
    std::vector<ecs::Entity> finished;
    registry.runSystem<ExplosionBurst, ecs::Transform, ecs::Renderable>(
        [deltaTime, &finished](const ecs::Entity entity, ExplosionBurst& burst, ecs::Transform& transform,
                               ecs::Renderable& renderable) {
            if (burst.delay > 0.0F) {
                burst.delay -= deltaTime;
                renderable.tint.a = 0;
                return;
            }
            burst.age += deltaTime;
            const float progress = std::min(1.0F, burst.age / burst.duration);
            const float scale = burst.startScale * (1.0F + ((burst.growth - 1.0F) * progress));
            transform.scaleX = scale;
            transform.scaleY = scale;
            renderable.tint = burst.tint;
            renderable.tint.a = static_cast<std::uint8_t>(static_cast<float>(burst.tint.a) * (1.0F - progress));
            if (burst.age >= burst.duration) {
                finished.push_back(entity);
            }
        });
    for (const ecs::Entity entity : finished) {
        registry.killEntity(entity);
    }
}

}  // namespace client::gameplay
