#include <gtest/gtest.h>

#include <numbers>

#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "gameplay/FaceMotion.hpp"

namespace {

namespace gameplay = client::gameplay;

constexpr float PI = std::numbers::pi_v<float>;

}  // namespace

TEST(FaceMotion, TurnsTowardsTheMove) {
    ecs::Registry registry;
    const ecs::Entity shot = registry.spawnEntity();
    registry.addComponent(shot, ecs::Transform{.x = 100.0F, .y = 100.0F});
    registry.addComponent(shot, gameplay::FaceMotion{});
    auto& transform = registry.getComponents<ecs::Transform>().get(shot);

    gameplay::faceMotion(registry);
    EXPECT_FLOAT_EQ(transform.rotation, 0.0F) << "no previous position yet";

    transform.y += 5.0F;  // straight down
    gameplay::faceMotion(registry);
    EXPECT_NEAR(transform.rotation, PI / 2.0F, 1e-5);

    gameplay::faceMotion(registry);
    EXPECT_NEAR(transform.rotation, PI / 2.0F, 1e-5) << "still: keeps its direction";
}

TEST(FaceMotion, AMirroredSpriteTurnsItsLeftSideTowardsTheMove) {
    ecs::Registry registry;
    const ecs::Entity shot = registry.spawnEntity();
    registry.addComponent(shot, ecs::Transform{.x = 100.0F, .y = 100.0F});
    registry.addComponent(shot, gameplay::FaceMotion{.forward = PI});
    auto& transform = registry.getComponents<ecs::Transform>().get(shot);
    gameplay::faceMotion(registry);

    transform.x -= 5.0F;  // straight left: the way the mirrored sheet already faces
    gameplay::faceMotion(registry);
    EXPECT_NEAR(transform.rotation, 0.0F, 1e-5);
}
