#include <gtest/gtest.h>

#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

TEST(Renderable, DefaultValues) {
    const ecs::Renderable renderable;

    EXPECT_EQ(renderable.textureId, ecs::NO_TEXTURE);
    EXPECT_EQ(renderable.sourceRect.x, 0);
    EXPECT_EQ(renderable.sourceRect.y, 0);
    EXPECT_EQ(renderable.sourceRect.width, 0);
    EXPECT_EQ(renderable.sourceRect.height, 0);
    EXPECT_EQ(renderable.layer, 0);
    EXPECT_EQ(renderable.tint.r, 255);
    EXPECT_EQ(renderable.tint.g, 255);
    EXPECT_EQ(renderable.tint.b, 255);
    EXPECT_EQ(renderable.tint.a, 255);
}

TEST(Renderable, DesignatedInitializers) {
    const ecs::Renderable renderable{
        .textureId = 7,
        .sourceRect = {.x = 32, .y = 16, .width = 24, .height = 24},
        .tint = {.r = 255, .g = 0, .b = 0, .a = 128},
        .layer = 3,
    };

    EXPECT_EQ(renderable.textureId, 7U);
    EXPECT_EQ(renderable.sourceRect.x, 32);
    EXPECT_EQ(renderable.sourceRect.y, 16);
    EXPECT_EQ(renderable.sourceRect.width, 24);
    EXPECT_EQ(renderable.sourceRect.height, 24);
    EXPECT_EQ(renderable.tint.g, 0);
    EXPECT_EQ(renderable.tint.a, 128);
    EXPECT_EQ(renderable.layer, 3);
}

TEST(Renderable, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    registry.addComponent<ecs::Renderable>(entity, ecs::Renderable{.textureId = 2, .layer = -1});

    ASSERT_TRUE(registry.hasComponent<ecs::Renderable>(entity));
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(entity).layer, -1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
