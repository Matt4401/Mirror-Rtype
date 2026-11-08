#include <gtest/gtest.h>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/AimedSprite.hpp"

namespace {

namespace gameplay = client::gameplay;

/** @brief A 16-direction turret whose first frame faces 22.5 degrees, like a strip starting just after "right". */
constexpr gameplay::AimedSprite TURRET{
    .startX = 0, .startY = 0, .frameWidth = 33, .frameHeight = 32, .frames = 16, .firstAngle = 22.5F};

}  // namespace

TEST(AimedSprite, PicksTheFrameNearestToTheDirection) {
    EXPECT_EQ(gameplay::aimedFrame(TURRET, 22.5F), 0);
    EXPECT_EQ(gameplay::aimedFrame(TURRET, 90.0F), 3) << "down";
    EXPECT_EQ(gameplay::aimedFrame(TURRET, 180.0F), 7) << "left";
    EXPECT_EQ(gameplay::aimedFrame(TURRET, -90.0F), 11) << "up";
    EXPECT_EQ(gameplay::aimedFrame(TURRET, 0.0F), 15) << "right";
    EXPECT_EQ(gameplay::aimedFrame(TURRET, 359.0F), 15);
    EXPECT_EQ(gameplay::aimedFrame(TURRET, 30.0F), 0) << "rounded to the nearest frame";
}

TEST(AimedSprite, FacesTheNearestShip) {
    ecs::Registry registry;
    const ecs::Entity turret = registry.spawnEntity();
    registry.addComponent(turret, ecs::Transform{.x = 500.0F, .y = 300.0F});
    registry.addComponent(turret, ecs::Renderable{});
    registry.addComponent(turret, TURRET);

    gameplay::aimSprites(registry);
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(turret).sourceRect.width, 0) << "no ship: unchanged";

    const auto ship = [&registry](const float x, const float y) {
        const ecs::Entity entity = registry.spawnEntity();
        registry.emplaceComponent<ecs::Replicated>(entity, rtype::game::KIND_PLAYER_SHIP);
        registry.addComponent(entity, ecs::Transform{.x = x, .y = y});
    };
    ship(100.0F, 300.0F);  // to the left, near
    ship(500.0F, 700.0F);  // below, farther
    gameplay::aimSprites(registry);
    const auto& source = registry.getComponents<ecs::Renderable>().get(turret).sourceRect;
    EXPECT_EQ(source.x, 7 * 33) << "the left frame";
    EXPECT_EQ(source.width, 33);
}
