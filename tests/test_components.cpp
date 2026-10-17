#include <gtest/gtest.h>

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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
