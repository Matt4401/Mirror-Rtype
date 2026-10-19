#include <gtest/gtest.h>

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/power_up/PowerUp.hpp"

namespace {

TEST(PrefabManager, UnknownPrefabQueuesNothing) {
    prefabs::PrefabManager manager;
    ecs::CommandBuffer buffer;

    EXPECT_FALSE(manager.has("does_not_exist"));
    EXPECT_FALSE(manager.spawn("does_not_exist", buffer, 0.0F, 0.0F));
    EXPECT_TRUE(buffer.empty());
}

TEST(PrefabManager, SpawnIsDeferredUntilFlush) {
    prefabs::PrefabManager manager;
    prefabs::registerMissilePrefab(manager);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(prefabs::MISSILE_PREFAB, buffer, 0.0F, 0.0F));
    EXPECT_FALSE(buffer.empty());
    EXPECT_FALSE(registry.isAlive(0));

    buffer.flush(registry);

    EXPECT_TRUE(buffer.empty());
    EXPECT_TRUE(registry.isAlive(0));
}

TEST(PrefabManager, MissileHasAllComponents) {
    prefabs::PrefabManager manager;
    prefabs::registerMissilePrefab(manager);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::MISSILE_PREFAB, buffer, 800.0F, 300.0F);
    buffer.flush(registry);
    ecs::Entity const missile = 0;

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(missile));
    ASSERT_TRUE(registry.hasComponent<ecs::Renderable>(missile));
    ASSERT_TRUE(registry.hasComponent<ecs::RigidBody2D>(missile));
    ASSERT_TRUE(registry.hasComponent<ecs::Collider2D>(missile));

    auto const& transform = registry.getComponents<ecs::Transform>().get(missile);
    EXPECT_FLOAT_EQ(transform.x, 800.0F);
    EXPECT_FLOAT_EQ(transform.y, 300.0F);
    auto const& body = registry.getComponents<ecs::RigidBody2D>().get(missile);
    EXPECT_FLOAT_EQ(body.velocityX, prefabs::DEFAULT_MISSILE_SPEED);
    EXPECT_TRUE(body.bullet);
}

TEST(PrefabManager, MissileVariantUsesCustomSpeed) {
    prefabs::PrefabManager manager;
    prefabs::registerMissilePrefab(manager);
    prefabs::registerMissilePrefab(manager, -1200.0F, "fast_missile");
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::MISSILE_PREFAB, buffer, 0.0F, 0.0F);
    manager.spawn("fast_missile", buffer, 0.0F, 0.0F);
    buffer.flush(registry);

    auto const& bodies = registry.getComponents<ecs::RigidBody2D>();
    EXPECT_FLOAT_EQ(bodies.get(0).velocityX, prefabs::DEFAULT_MISSILE_SPEED);
    EXPECT_FLOAT_EQ(bodies.get(1).velocityX, -1200.0F);
}

TEST(PrefabManager, SalvoOfFiveMissilesDuringIteration) {
    prefabs::PrefabManager manager;
    prefabs::registerMissilePrefab(manager);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ecs::Entity const boss = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(boss, ecs::Transform{.x = 900.0F, .y = 400.0F});

    // Spawning while iterating a sparse array is safe: nothing is created until flush.
    for (auto const& transform : registry.getComponents<ecs::Transform>().getDense()) {
        for (int i = 0; i < 5; ++i) {
            manager.spawn(prefabs::MISSILE_PREFAB, buffer, transform.x, transform.y + static_cast<float>(i * 20));
        }
    }
    EXPECT_EQ(registry.getComponents<ecs::Transform>().getDense().size(), 1U);

    buffer.flush(registry);

    EXPECT_EQ(registry.getComponents<ecs::RigidBody2D>().getDense().size(), 5U);
    EXPECT_EQ(registry.getComponents<ecs::Transform>().getDense().size(), 6U);
}

TEST(PrefabManager, PowerUpIsDriftingSensor) {
    prefabs::PrefabManager manager;
    prefabs::registerPowerUpPrefab(manager);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(prefabs::POWER_UP_PREFAB, buffer, 500.0F, 200.0F));
    buffer.flush(registry);
    ecs::Entity const powerUp = 0;

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(powerUp));
    ASSERT_TRUE(registry.hasComponent<ecs::Renderable>(powerUp));
    ASSERT_TRUE(registry.hasComponent<ecs::RigidBody2D>(powerUp));
    ASSERT_TRUE(registry.hasComponent<ecs::Collider2D>(powerUp));

    auto const& body = registry.getComponents<ecs::RigidBody2D>().get(powerUp);
    EXPECT_EQ(body.bodyType, ecs::BodyType::Kinematic);
    EXPECT_FLOAT_EQ(body.velocityX, prefabs::DEFAULT_POWER_UP_DRIFT_SPEED);
    auto const& collider = registry.getComponents<ecs::Collider2D>().get(powerUp);
    EXPECT_EQ(collider.shape, ecs::ColliderShape::Circle);
    EXPECT_TRUE(collider.isSensor);
}

TEST(PrefabManager, ObstacleIsStaticSolidBox) {
    prefabs::PrefabManager manager;
    prefabs::registerObstaclePrefab(manager);
    prefabs::registerObstaclePrefab(manager, 128.0F, 16.0F, "obstacle_wide");
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::OBSTACLE_PREFAB, buffer, 300.0F, 400.0F);
    manager.spawn("obstacle_wide", buffer, 600.0F, 400.0F);
    buffer.flush(registry);

    auto const& bodies = registry.getComponents<ecs::RigidBody2D>();
    EXPECT_EQ(bodies.get(0).bodyType, ecs::BodyType::Static);
    auto const& colliders = registry.getComponents<ecs::Collider2D>();
    EXPECT_FALSE(colliders.get(0).isSensor);
    EXPECT_FLOAT_EQ(colliders.get(0).halfWidth, prefabs::DEFAULT_OBSTACLE_HALF_WIDTH);
    EXPECT_FLOAT_EQ(colliders.get(1).halfWidth, 128.0F);
    EXPECT_FLOAT_EQ(colliders.get(1).halfHeight, 16.0F);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
