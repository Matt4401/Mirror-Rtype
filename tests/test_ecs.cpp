#include <gtest/gtest.h>
#include "ecs/Registry.hpp"

struct Position { float x, y; };

TEST(ECS, LifecycleAndIteration) {
    ecs::Registry registry;
    registry.register_component<Position>();

    ecs::Entity e1 = registry.spawn_entity();
    registry.add_component<Position>(e1, {.x=10.0F, .y=20.0F});

    auto& positions = registry.get_components<Position>();
    EXPECT_TRUE(positions.contains(e1));
    EXPECT_EQ(positions.get(e1).x, 10.0F);

    registry.kill_entity(e1);
    EXPECT_FALSE(positions.contains(e1));
}

TEST(ECS, EntityRecycling) {
    ecs::Registry registry;

    ecs::Entity e1 = registry.spawn_entity();
    ecs::Entity e2 = registry.spawn_entity();

    EXPECT_EQ(e1, 0);
    EXPECT_EQ(e2, 1);

    registry.kill_entity(e1);

    ecs::Entity e3 = registry.spawn_entity();
    EXPECT_EQ(e3, 0); // Should recycle e1's ID
}

TEST(ECS, SparseArraySwapAndPop) {
    ecs::Registry registry;
    registry.register_component<Position>();

    ecs::Entity e1 = registry.spawn_entity();
    ecs::Entity e2 = registry.spawn_entity();
    ecs::Entity e3 = registry.spawn_entity();

    registry.add_component<Position>(e1, {.x=1.0F, .y=1.0F});
    registry.add_component<Position>(e2, {.x=2.0F, .y=2.0F});
    registry.add_component<Position>(e3, {.x=3.0F, .y=3.0F});

    auto& positions = registry.get_components<Position>();
    EXPECT_EQ(positions.get_dense().size(), 3);

    // Erasing e2 should swap e3 into e2's place in the dense array
    registry.kill_entity(e2);

    EXPECT_FALSE(positions.contains(e2));
    EXPECT_TRUE(positions.contains(e1));
    EXPECT_TRUE(positions.contains(e3));
    EXPECT_EQ(positions.get_dense().size(), 2);

    // Ensure e3's data is intact after being swapped
    EXPECT_EQ(positions.get(e3).x, 3.0F);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
