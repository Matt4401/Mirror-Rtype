#include <gtest/gtest.h>

#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

TEST(Transform, DefaultValues) {
    const ecs::Transform transform;

    EXPECT_EQ(transform.x, 0.0F);
    EXPECT_EQ(transform.y, 0.0F);
    EXPECT_EQ(transform.z, 0.0F);
    EXPECT_EQ(transform.rotation, 0.0F);
    EXPECT_EQ(transform.scaleX, 1.0F);
    EXPECT_EQ(transform.scaleY, 1.0F);
}

TEST(Transform, DesignatedInitializersKeepOtherDefaults) {
    const ecs::Transform transform{.x = 100.0F, .y = 200.0F, .z = 3.0F};

    EXPECT_EQ(transform.x, 100.0F);
    EXPECT_EQ(transform.y, 200.0F);
    EXPECT_EQ(transform.z, 3.0F);
    EXPECT_EQ(transform.scaleX, 1.0F);
    EXPECT_EQ(transform.scaleY, 1.0F);
}

TEST(Transform, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<ecs::Transform>(entity, ecs::Transform{.x = 1.0F, .y = 2.0F});

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(entity));
    EXPECT_EQ(registry.getComponents<ecs::Transform>().get(entity).y, 2.0F);

    registry.runSystem<ecs::Transform>(
        []([[maybe_unused]] ecs::Entity e, ecs::Transform& transform) { transform.x += 10.0F; });
    EXPECT_EQ(registry.getComponents<ecs::Transform>().get(entity).x, 11.0F);
}

TEST(RigidBody2D, DefaultValues) {
    const ecs::RigidBody2D body;

    EXPECT_EQ(body.bodyType, ecs::BodyType::Static);
    EXPECT_EQ(body.velocityX, 0.0F);
    EXPECT_EQ(body.velocityY, 0.0F);
    EXPECT_EQ(body.linearDamping, 0.0F);
    EXPECT_EQ(body.angularDamping, 0.0F);
    EXPECT_EQ(body.maxLinearSpeed, 2000.0F);
    EXPECT_TRUE(body.fixedRotation);
    EXPECT_FALSE(body.bullet);
}

TEST(RigidBody2D, DesignatedInitializersKeepOtherDefaults) {
    const ecs::RigidBody2D body{
        .bodyType = ecs::BodyType::Dynamic,
        .linearDamping = 0.5F,
    };

    EXPECT_EQ(body.bodyType, ecs::BodyType::Dynamic);
    EXPECT_EQ(body.linearDamping, 0.5F);
    EXPECT_EQ(body.velocityX, 0.0F);
    EXPECT_TRUE(body.fixedRotation);
}

TEST(Collider2D, DefaultValues) {
    const ecs::Collider2D collider;

    EXPECT_EQ(collider.shape, ecs::ColliderShape::Box);
    EXPECT_EQ(collider.halfWidth, 16.0F);
    EXPECT_EQ(collider.halfHeight, 16.0F);
    EXPECT_EQ(collider.radius, 16.0F);
    EXPECT_EQ(collider.density, 1.0F);
    EXPECT_EQ(collider.friction, 0.2F);
    EXPECT_EQ(collider.restitution, 0.0F);
    EXPECT_EQ(collider.categoryBits, 0x0001);
    EXPECT_EQ(collider.maskBits, 0xFFFF);
    EXPECT_FALSE(collider.isSensor);
}

TEST(Collider2D, DesignatedInitializersKeepOtherDefaults) {
    const ecs::Collider2D collider{
        .shape = ecs::ColliderShape::Circle,
        .radius = 24.0F,
        .density = 0.5F,
        .restitution = 0.75F,
    };

    EXPECT_EQ(collider.shape, ecs::ColliderShape::Circle);
    EXPECT_EQ(collider.radius, 24.0F);
    EXPECT_EQ(collider.density, 0.5F);
    EXPECT_EQ(collider.restitution, 0.75F);
    EXPECT_EQ(collider.friction, 0.2F);
    EXPECT_EQ(collider.maskBits, 0xFFFF);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
