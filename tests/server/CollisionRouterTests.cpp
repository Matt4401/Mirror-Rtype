#include <gtest/gtest.h>

#include <cstdint>
#include <variant>
#include <vector>

#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "events/EventBus.hpp"
#include "events/PhysicsEvents.hpp"
#include "prefabs/CollisionCategories.hpp"

namespace {

namespace category = prefabs::collision;
using rtype::server::collision::classify;
using rtype::server::collision::CollisionRouter;
using rtype::server::collision::PowerUpTouchedEvent;
using rtype::server::collision::ShipContactEvent;
using rtype::server::collision::ShotHitEvent;

events::CollisionBeginEvent contact(ecs::Entity a, std::uint16_t categoryA, ecs::Entity b, std::uint16_t categoryB) {
    return events::CollisionBeginEvent{
        .entityA = a, .entityB = b, .categoryA = categoryA, .categoryB = categoryB, .isSensor = true};
}

TEST(CollisionRouter, PlayerShotHitsEnemyInEitherOrder) {
    for (const auto& event : {contact(1, category::PLAYER_SHOT, 2, category::ENEMY),
                              contact(2, category::ENEMY, 1, category::PLAYER_SHOT)}) {
        const auto result = classify(event);
        ASSERT_TRUE(result.has_value());
        const auto* hit = std::get_if<ShotHitEvent>(&*result);
        ASSERT_NE(hit, nullptr);
        EXPECT_EQ(hit->shot, 1U);
        EXPECT_EQ(hit->target, 2U);
        EXPECT_EQ(hit->targetCategory, category::ENEMY);
        EXPECT_TRUE(hit->byPlayer);
    }
}

TEST(CollisionRouter, EnemyShotHitsPlayer) {
    const auto result = classify(contact(3, category::PLAYER, 4, category::ENEMY_SHOT));
    ASSERT_TRUE(result.has_value());
    const auto* hit = std::get_if<ShotHitEvent>(&*result);
    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(hit->shot, 4U);
    EXPECT_EQ(hit->target, 3U);
    EXPECT_FALSE(hit->byPlayer);
}

TEST(CollisionRouter, ShipTouchesPowerUpAndObstacle) {
    const auto powerUp = classify(contact(5, category::POWER_UP, 6, category::PLAYER));
    ASSERT_TRUE(powerUp.has_value());
    const auto* touched = std::get_if<PowerUpTouchedEvent>(&*powerUp);
    ASSERT_NE(touched, nullptr);
    EXPECT_EQ(touched->ship, 6U);
    EXPECT_EQ(touched->powerUp, 5U);

    const auto wall = classify(contact(6, category::PLAYER, 7, category::OBSTACLE));
    ASSERT_TRUE(wall.has_value());
    const auto* ship = std::get_if<ShipContactEvent>(&*wall);
    ASSERT_NE(ship, nullptr);
    EXPECT_EQ(ship->ship, 6U);
    EXPECT_EQ(ship->other, 7U);
    EXPECT_EQ(ship->otherCategory, category::OBSTACLE);
}

TEST(CollisionRouter, UnknownPairIsIgnored) {
    EXPECT_FALSE(classify(contact(1, category::ENEMY, 2, category::OBSTACLE)).has_value());
    EXPECT_FALSE(classify(contact(1, category::PLAYER_SHOT, 2, category::ENEMY_SHOT)).has_value());
}

TEST(CollisionRouter, RepublishesOnTheBusUntilDestroyed) {
    ecs::EventBus bus;
    std::vector<ShotHitEvent> hits;
    bus.subscribe<ShotHitEvent>([&](const ShotHitEvent& event) { hits.push_back(event); });

    {
        const CollisionRouter router(bus);
        bus.publish(contact(1, category::PLAYER_SHOT, 2, category::ENEMY));
        bus.publish(contact(1, category::ENEMY, 2, category::OBSTACLE));
    }
    bus.publish(contact(1, category::PLAYER_SHOT, 2, category::ENEMY));

    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits.front().target, 2U);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
