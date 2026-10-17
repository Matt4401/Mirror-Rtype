#include <gtest/gtest.h>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "render/CameraComponent.hpp"

TEST(CameraComponent, DefaultValues) {
    const render::CameraComponent component;

    EXPECT_TRUE(component.active);
    EXPECT_EQ(component.camera.zoom, 1.0F);
    EXPECT_EQ(component.camera.rotation, 0.0F);
    EXPECT_EQ(component.camera.target.x, 0.0F);
    EXPECT_EQ(component.camera.target.y, 0.0F);
    EXPECT_EQ(component.camera.offset.x, 0.0F);
    EXPECT_EQ(component.camera.offset.y, 0.0F);
}

TEST(CameraComponent, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    render::CameraComponent component;
    component.camera.target = {.x = 400.0F, .y = 300.0F};
    component.camera.zoom = 2.0F;
    registry.addComponent<render::CameraComponent>(entity, component);

    ASSERT_TRUE(registry.hasComponent<render::CameraComponent>(entity));
    const auto& stored = registry.getComponents<render::CameraComponent>().get(entity);
    EXPECT_EQ(stored.camera.target.x, 400.0F);
    EXPECT_EQ(stored.camera.zoom, 2.0F);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
