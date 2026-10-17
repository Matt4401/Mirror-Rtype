#include <gtest/gtest.h>

#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Registry.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace {

constexpr float FIXED_DT = 1.0F / 60.0F;

TEST(PhysicsSystem, DynamicBodyAdvancesTransform) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(
        entity, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 300.0F});

    physicsSystem.update(FIXED_DT);

    EXPECT_NEAR(registry.getComponents<ecs::Transform>().get(entity).x, 5.0F, 0.001F);
    EXPECT_EQ(physicsSystem.bodyCount(), 1U);
}

TEST(PhysicsSystem, DynamicBodyStopsAtStaticWall) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);

    const ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(wall, ecs::Transform{.x = 100.0F});
    registry.addComponent<ecs::RigidBody2D>(wall, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(
        wall, ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});

    const ecs::Entity movingBody = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(movingBody, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(movingBody,
                                            ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic,
                                                              .velocityX = 600.0F,
                                                              .maxLinearSpeed = 1000.0F});
    registry.addComponent<ecs::Collider2D>(
        movingBody, ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 8.0F});

    for (int step = 0; step < 60; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    const ecs::Transform& transform = registry.getComponents<ecs::Transform>().get(movingBody);
    const ecs::RigidBody2D& rigidBody = registry.getComponents<ecs::RigidBody2D>().get(movingBody);
    EXPECT_LE(transform.x, 87.1F);
    EXPECT_NEAR(rigidBody.velocityX, 0.0F, 0.1F);
}

TEST(PhysicsSystem, FastBulletDoesNotPassThroughThinWall) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);

    const ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(wall, ecs::Transform{.x = 100.0F});
    registry.addComponent<ecs::RigidBody2D>(wall, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(wall, ecs::Collider2D{.halfWidth = 1.0F, .halfHeight = 50.0F});

    const ecs::Entity projectile = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(projectile, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(projectile,
                                            ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic,
                                                              .velocityX = 6000.0F,
                                                              .maxLinearSpeed = 7000.0F,
                                                              .bullet = true});
    registry.addComponent<ecs::Collider2D>(
        projectile, ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 2.0F});

    for (int step = 0; step < 4; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    EXPECT_LE(registry.getComponents<ecs::Transform>().get(projectile).x, 97.1F);
}

TEST(PhysicsSystem, EntityDestructionDestroysBody) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(entity, ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    registry.killEntity(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}