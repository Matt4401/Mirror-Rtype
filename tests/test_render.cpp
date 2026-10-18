#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "systems/render/RenderSystem.hpp"
#include "render/texture/TextureStore.hpp"
#include "render/window/Window.hpp"

TEST(CameraComponent, DefaultValues) {
    const ecs::CameraComponent component;

    EXPECT_TRUE(component.active);
    EXPECT_EQ(component.zoom, 1.0F);
    EXPECT_EQ(component.rotation, 0.0F);
    EXPECT_EQ(component.targetX, 0.0F);
    EXPECT_EQ(component.targetY, 0.0F);
    EXPECT_EQ(component.offsetX, 0.0F);
    EXPECT_EQ(component.offsetY, 0.0F);
}

TEST(CameraComponent, WorksAsEcsComponent) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    ecs::CameraComponent component{.targetX = 400.0F, .targetY = 300.0F, .zoom = 2.0F};
    registry.addComponent<ecs::CameraComponent>(entity, component);

    ASSERT_TRUE(registry.hasComponent<ecs::CameraComponent>(entity));
    const auto& stored = registry.getComponents<ecs::CameraComponent>().get(entity);
    EXPECT_EQ(stored.targetX, 400.0F);
    EXPECT_EQ(stored.zoom, 2.0F);
}

namespace {

ecs::Entity spawnDrawable(ecs::Registry& registry, int layer, float z, ecs::TextureId textureId = ecs::NO_TEXTURE) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{.z = z});
    registry.addComponent<ecs::Renderable>(entity, ecs::Renderable{.textureId = textureId, .layer = layer});
    return entity;
}

std::vector<ecs::Entity> drawOrder(const render::RenderSystem& system) {
    std::vector<ecs::Entity> order;
    for (const render::DrawItem& item : system.drawQueue()) {
        order.push_back(item.entity);
    }
    return order;
}

}  // namespace

TEST(RenderSystem, SortsByLayerThenDepthThenTextureThenEntity) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);

    const ecs::Entity front = spawnDrawable(registry, 2, 0.0F);
    const ecs::Entity middleNear = spawnDrawable(registry, 1, 5.0F);
    const ecs::Entity middleFar = spawnDrawable(registry, 1, -5.0F);
    const ecs::Entity back = spawnDrawable(registry, 0, 9.0F);
    const ecs::Entity tieSecond = spawnDrawable(registry, 3, 1.0F);
    const ecs::Entity tieFirst = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(tieFirst, ecs::Transform{.z = 1.0F});
    registry.addComponent<ecs::Renderable>(tieFirst, ecs::Renderable{.layer = 3});

    system.collect(registry);

    const std::vector<ecs::Entity> expected{back, middleFar, middleNear, front, tieSecond, tieFirst};
    EXPECT_EQ(drawOrder(system), expected);
}

TEST(RenderSystem, GroupsTexturesOnlyAtMatchingLayerAndDepth) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);

    const ecs::Entity textureNine = spawnDrawable(registry, 1, 2.0F, 9);
    const ecs::Entity textureThreeFirst = spawnDrawable(registry, 1, 2.0F, 3);
    const ecs::Entity textureThreeSecond = spawnDrawable(registry, 1, 2.0F, 3);
    const ecs::Entity otherDepth = spawnDrawable(registry, 1, 1.0F, 9);
    const ecs::Entity otherLayer = spawnDrawable(registry, 0, 2.0F, 9);

    system.collect(registry);

    const std::vector<ecs::Entity> expected{
        otherLayer, otherDepth, textureThreeFirst, textureThreeSecond, textureNine};
    EXPECT_EQ(drawOrder(system), expected);
}

TEST(RenderSystem, IgnoresEntitiesMissingAComponent) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);

    const ecs::Entity drawable = spawnDrawable(registry, 0, 0.0F);
    const ecs::Entity onlyTransform = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(onlyTransform, ecs::Transform{});
    const ecs::Entity onlyRenderable = registry.spawnEntity();
    registry.addComponent<ecs::Renderable>(onlyRenderable, ecs::Renderable{});

    system.collect(registry);

    EXPECT_EQ(drawOrder(system), std::vector<ecs::Entity>{drawable});
}

TEST(RenderSystem, CopiesRenderableDataIntoTheQueue) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);

    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{.x = 100.0F, .y = 200.0F});
    registry.addComponent<ecs::Renderable>(
        entity,
        ecs::Renderable{.textureId = 4,
                        .sourceRect = {.x = 32, .y = 16, .width = 24, .height = 24},
                        .origin = {.x = 0.0F, .y = 1.0F},
                        .tint = {.r = 10, .g = 20, .b = 30, .a = 40},
                        .layer = 1});

    system.collect(registry);

    ASSERT_EQ(system.drawQueue().size(), 1U);
    const render::DrawItem& item = system.drawQueue().front();
    EXPECT_EQ(item.transform.x, 100.0F);
    EXPECT_EQ(item.transform.y, 200.0F);
    EXPECT_EQ(item.textureId, 4U);
    EXPECT_EQ(item.sourceRect.x, 32);
    EXPECT_EQ(item.sourceRect.y, 16);
    EXPECT_EQ(item.sourceRect.width, 24);
    EXPECT_EQ(item.sourceRect.height, 24);
    EXPECT_EQ(item.origin.x, 0.0F);
    EXPECT_EQ(item.origin.y, 1.0F);
    EXPECT_EQ(item.tint.b, 30);
    EXPECT_EQ(item.layer, 1);
}

TEST(RenderSystem, QueueIsRebuiltEachFrameWithoutReallocating) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);
    for (int i = 0; i < 200; ++i) {
        spawnDrawable(registry, i % 3, 0.0F);
    }

    system.collect(registry);
    const render::DrawItem* firstFrameStorage = system.drawQueue().data();
    system.collect(registry);

    EXPECT_EQ(system.drawQueue().size(), 200U);
    EXPECT_EQ(system.drawQueue().data(), firstFrameStorage);
}

TEST(RenderSystem, UsesTheFirstActiveCamera) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);

    const ecs::Entity inactive = registry.spawnEntity();
    ecs::CameraComponent inactiveCamera;
    inactiveCamera.active = false;
    inactiveCamera.zoom = 9.0F;
    registry.addComponent<ecs::CameraComponent>(inactive, inactiveCamera);

    const ecs::Entity active = registry.spawnEntity();
    ecs::CameraComponent activeCamera;
    activeCamera.zoom = 2.0F;
    registry.addComponent<ecs::CameraComponent>(active, activeCamera);

    system.collect(registry);

    EXPECT_EQ(system.camera().zoom, 2.0F);
}

TEST(RenderSystem, FallsBackToANeutralCameraWhenNoneIsActive) {
    ecs::Registry registry;
    const render::TextureStore textures;
    render::RenderSystem system(textures);

    system.collect(registry);

    EXPECT_EQ(system.camera().zoom, 1.0F);
    EXPECT_EQ(system.camera().target.x, 0.0F);
}

TEST(TextureStore, UnknownIdentifiersResolveToNothing) {
    const render::TextureStore textures;

    EXPECT_EQ(textures.get(ecs::NO_TEXTURE), nullptr);
    EXPECT_EQ(textures.get(1), nullptr);
}

TEST(Window, RejectsNonPositiveDimensions) {
    EXPECT_THROW(render::Window(render::WindowConfig{.width = 0, .height = 720}), std::invalid_argument);
    EXPECT_THROW(render::Window(render::WindowConfig{.width = 1280, .height = -1}), std::invalid_argument);
}

TEST(Window, RejectsNonPositiveTargetFps) {
    EXPECT_THROW(render::Window(render::WindowConfig{.targetFps = 0}), std::invalid_argument);
    EXPECT_THROW(render::Window(render::WindowConfig{.targetFps = -1}), std::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
