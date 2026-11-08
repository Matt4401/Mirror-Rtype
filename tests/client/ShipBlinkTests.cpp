#include <gtest/gtest.h>

#include <cstdint>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "gameplay/ShipBlink.hpp"

using client::gameplay::BLINK_FADED_ALPHA;
using client::gameplay::BLINK_PERIOD;
using client::gameplay::blinkAlpha;
using client::gameplay::OPAQUE_ALPHA;

namespace {

constexpr std::uint8_t INVULNERABLE = rtype::game::FLAG_ALIVE | rtype::game::FLAG_INVULNERABLE;

ecs::Entity spawn(ecs::Registry& registry, std::uint32_t kind, std::uint8_t flags, std::uint8_t alpha) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, ecs::Replicated{.kind = kind});
    registry.addComponent(entity, rtype::game::EntityNetState{.flags = flags});
    registry.addComponent(entity, ecs::Renderable{.tint = {.a = alpha}});
    return entity;
}

}  // namespace

TEST(ShipBlink, VulnerableEntitiesStayOpaque) {
    EXPECT_EQ(blinkAlpha(rtype::game::FLAG_ALIVE, 0.0F), OPAQUE_ALPHA);
    EXPECT_EQ(blinkAlpha(rtype::game::FLAG_ALIVE, BLINK_PERIOD * 0.75F), OPAQUE_ALPHA);
}

TEST(ShipBlink, InvulnerableEntitiesAlternate) {
    EXPECT_EQ(blinkAlpha(INVULNERABLE, BLINK_PERIOD * 0.25F), OPAQUE_ALPHA);
    EXPECT_EQ(blinkAlpha(INVULNERABLE, BLINK_PERIOD * 0.75F), BLINK_FADED_ALPHA);
    EXPECT_EQ(blinkAlpha(INVULNERABLE, BLINK_PERIOD * 1.25F), OPAQUE_ALPHA);
}

TEST(ShipBlink, OnlyShipsBlinkAndTheyComeBackOpaque) {
    ecs::Registry registry;
    const ecs::Entity ship = spawn(registry, rtype::game::KIND_PLAYER_SHIP, INVULNERABLE, OPAQUE_ALPHA);
    const ecs::Entity boss = spawn(registry, rtype::game::KIND_BOSS_CORE, INVULNERABLE, 200);
    auto& renderables = registry.getComponents<ecs::Renderable>();

    client::gameplay::blinkInvulnerableShips(registry, BLINK_PERIOD * 0.75F);
    EXPECT_EQ(renderables.get(ship).tint.a, BLINK_FADED_ALPHA);
    EXPECT_EQ(renderables.get(boss).tint.a, 200) << "other entities keep their own opacity";

    registry.getComponents<rtype::game::EntityNetState>().get(ship).flags = rtype::game::FLAG_ALIVE;
    client::gameplay::blinkInvulnerableShips(registry, BLINK_PERIOD * 0.75F);
    EXPECT_EQ(renderables.get(ship).tint.a, OPAQUE_ALPHA);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
