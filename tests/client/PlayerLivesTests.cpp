#include <gtest/gtest.h>

#include <cstdint>

#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/PlayerLives.hpp"
#include "gameplay/ShipBlink.hpp"

namespace {

namespace gameplay = client::gameplay;

ecs::Entity spawnShip(ecs::Registry& registry, const std::uint8_t player, const std::uint8_t lives,
                      const std::uint8_t flags = rtype::game::FLAG_ALIVE) {
    const ecs::Entity ship = registry.spawnEntity();
    registry.emplaceComponent<ecs::Replicated>(ship, rtype::game::KIND_PLAYER_SHIP);
    registry.addComponent(ship, rtype::game::EntityNetState{.subtype = player, .health = lives, .flags = flags});
    return ship;
}

}  // namespace

TEST(PlayerLives, ReadsTheLivesOfTheOwnShip) {
    ecs::Registry registry;
    EXPECT_EQ(gameplay::ownShipStatus(registry, 1).lives, gameplay::STARTING_LIVES) << "before the first snapshot";
    spawnShip(registry, 0, 3);
    spawnShip(registry, 1, 2);
    const gameplay::OwnShipStatus status = gameplay::ownShipStatus(registry, 1);
    EXPECT_EQ(status.lives, 2);
    EXPECT_FALSE(status.eliminated);
}

TEST(PlayerLives, SeesTheEliminationOfTheOwnShip) {
    ecs::Registry registry;
    spawnShip(registry, 0, 0, rtype::game::FLAG_ALIVE | rtype::game::FLAG_ELIMINATED);
    EXPECT_TRUE(gameplay::ownShipStatus(registry, 0).eliminated);
    EXPECT_EQ(gameplay::blinkAlpha(rtype::game::FLAG_ELIMINATED, 0.0F), 0) << "an eliminated ship is not drawn";
}

TEST(PlayerLives, TheIconIsTheShipAtRestInThePlayerRow) {
    const gameplay::ShipIconSheet sheet{.texture = 1, .frameWidth = 33, .frameHeight = 17, .frames = 5};
    const ecs::TextureRect frame = gameplay::idleShipFrame(sheet, 2);
    EXPECT_EQ(frame.x, 66) << "the middle of 5 frames";
    EXPECT_EQ(frame.y, 34) << "the third row";
    EXPECT_EQ(frame.width, 33);
    EXPECT_EQ(frame.height, 17);
}
