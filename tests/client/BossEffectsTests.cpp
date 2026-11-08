#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/BossEffects.hpp"

namespace {

namespace gameplay = client::gameplay;

constexpr ecs::Rgba BASE_TINT = {.r = 10, .g = 20, .b = 30, .a = 255};

/** @brief A boss as the client holds it: replicated hit points, a sprite, a flash. */
ecs::Entity spawnBoss(ecs::Registry& registry, const std::uint8_t health) {
    const ecs::Entity boss = registry.spawnEntity();
    registry.addComponent(boss, rtype::game::EntityNetState{.health = health});
    registry.addComponent(boss, ecs::Renderable{.tint = BASE_TINT});
    registry.addComponent(boss, gameplay::HitFlash{.spawnHealth = health, .lastHealth = health, .baseTint = BASE_TINT});
    return boss;
}

}  // namespace

TEST(BossEffects, LosingHitPointsFlashesForAMoment) {
    ecs::Registry registry;
    const ecs::Entity boss = spawnBoss(registry, 40);
    gameplay::updateHitFlashes(registry, 0.016F);
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(boss).tint.g, BASE_TINT.g);

    registry.getComponents<rtype::game::EntityNetState>().get(boss).health = 39;
    gameplay::updateHitFlashes(registry, 0.016F);
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(boss).tint.r, gameplay::HIT_FLASH_TINT.r);

    gameplay::updateHitFlashes(registry, gameplay::HIT_FLASH_DURATION);
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(boss).tint.r, BASE_TINT.r);
}

TEST(BossEffects, OnlyANearlyDeadBossCountsAsKilled) {
    EXPECT_TRUE(gameplay::diedInCombat({.spawnHealth = 40, .lastHealth = 1}));
    EXPECT_TRUE(gameplay::diedInCombat({.spawnHealth = 40, .lastHealth = 4}));
    EXPECT_FALSE(gameplay::diedInCombat({.spawnHealth = 40, .lastHealth = 20})) << "removed with its game";
}

TEST(BossEffects, ExplosionStartsAtTheCenterAndSpreadsOverTheBoss) {
    const std::vector<gameplay::BurstSpec> bursts = gameplay::explosionBursts(1000.0F, 360.0F, 200.0F, 100.0F);
    ASSERT_EQ(bursts.size(), static_cast<std::size_t>(gameplay::EXPLOSION_BURSTS));
    EXPECT_FLOAT_EQ(bursts.front().x, 1000.0F);
    EXPECT_FLOAT_EQ(bursts.front().delay, 0.0F);
    for (const gameplay::BurstSpec& burst : bursts) {
        EXPECT_GE(burst.x, 900.0F);
        EXPECT_LE(burst.x, 1100.0F);
        EXPECT_GE(burst.y, 310.0F);
        EXPECT_LE(burst.y, 410.0F);
    }
    EXPECT_NEAR(bursts.back().delay, gameplay::EXPLOSION_SPREAD, 1e-4);
}

TEST(BossEffects, BurstWaitsGrowsFadesThenDisappears) {
    ecs::Registry registry;
    const ecs::Entity burst = registry.spawnEntity();
    registry.addComponent(burst, ecs::Transform{});
    registry.addComponent(burst, ecs::Renderable{});
    registry.addComponent(burst, gameplay::ExplosionBurst{.delay = 0.1F, .startScale = 1.0F, .growth = 3.0F});

    gameplay::updateExplosions(registry, 0.05F);
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(burst).tint.a, 0) << "hidden during its delay";

    gameplay::updateExplosions(registry, 0.06F);
    gameplay::updateExplosions(registry, gameplay::EXPLOSION_BURST_DURATION / 2.0F);
    EXPECT_GT(registry.getComponents<ecs::Transform>().get(burst).scaleX, 1.5F);
    EXPECT_GT(registry.getComponents<ecs::Renderable>().get(burst).tint.a, 0);

    gameplay::updateExplosions(registry, gameplay::EXPLOSION_BURST_DURATION);
    EXPECT_FALSE(registry.isAlive(burst));
}

TEST(BossEffects, BossHealthSumsTheBossesOnScreen) {
    ecs::Registry registry;
    EXPECT_FALSE(gameplay::bossHealth(registry).has_value());
    for (const std::uint8_t health : {std::uint8_t{10}, std::uint8_t{30}}) {
        const ecs::Entity boss = spawnBoss(registry, 40);
        registry.emplaceComponent<ecs::Replicated>(boss, rtype::game::KIND_BOSS_CORE);
        registry.getComponents<rtype::game::EntityNetState>().get(boss).health = health;
    }
    const ecs::Entity turret = spawnBoss(registry, 8);
    registry.emplaceComponent<ecs::Replicated>(turret, rtype::game::KIND_BOSS_PART);

    const auto health = gameplay::bossHealth(registry);
    ASSERT_TRUE(health.has_value());
    EXPECT_FLOAT_EQ(health->points, 40.0F) << "turrets do not count";
    EXPECT_FLOAT_EQ(health->max, 80.0F);
}
