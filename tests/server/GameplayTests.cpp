#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/Replicated.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "gameplay/GameplayPlugin.hpp"
#include "level/Level.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace {

using rtype::server::gameplay::GameplayPlugin;
using rtype::server::gameplay::POINTS_PER_KILL;
namespace game = rtype::game;
namespace level = rtype::server::level;

constexpr float FIXED_DT = 1.0F / 60.0F;
/** @brief Height far from every enemy spawn height, so that the waves never get in the way. */
constexpr float LANE_Y = 40.0F;
/** @brief Scroll speed of the test level: 60 px/s, so 60 px of scroll is one second. */
constexpr float LEVEL_SPEED = 60.0F;
/** @brief Scroll speed set by the `scroll` event of the test level. */
constexpr float SLOW_SPEED = 30.0F;

/** @brief Spawn event of an alien scout, as the level loader builds it. */
timeline::TimelineEvent spawnAt(const float position, const float y) {
    return {.position = position,
            .type = std::string(level::EVENT_SPAWN),
            .params = {{"prefab", std::string("alien_scout")}, {"x", 1328.0F}, {"y", y}}};
}

/**
 * @brief Test level: enemies at 1 s and 2 s, the scroll slows down at 3 s, the level ends 2 s later (5 s).
 */
std::shared_ptr<const level::Level> testLevel() {
    auto built = std::make_shared<level::Level>();
    built->name = "Test";
    built->scrollSpeed = LEVEL_SPEED;
    built->timeline.add(spawnAt(60.0F, 160.0F));
    built->timeline.add(spawnAt(120.0F, 420.0F));
    built->timeline.add(
        {.position = 180.0F, .type = std::string(level::EVENT_SCROLL), .params = {{"speed", SLOW_SPEED}}});
    built->timeline.add({.position = 240.0F, .type = std::string(level::EVENT_END), .params = {}});
    return built;
}

/** @brief Room-like engine with a fake clock: physics, the level player and the R-Type rules, nothing else. */
class GameplayTest : public ::testing::Test {
  protected:
    explicit GameplayTest(std::shared_ptr<const level::Level> played = testLevel()) : engine([this] { return now; }) {
        engine.setFixedDeltaTime(FIXED_DT);
        engine.addPlugin<systems::PhysicsPlugin>();
        engine.addPlugin<timeline::TimelinePlugin>();
        engine.addPlugin<GameplayPlugin>(std::move(played));
        engine.init();
        gameplay().setPlayerCount(2);
    }

    [[nodiscard]] GameplayPlugin& gameplay() { return engine.getPlugin<GameplayPlugin>(); }
    [[nodiscard]] timeline::TimelinePlugin& player() { return engine.getPlugin<timeline::TimelinePlugin>(); }
    [[nodiscard]] ecs::Registry& registry() { return engine.registry(); }

    /** @brief Runs the engine for some seconds of game time. */
    void run(const float seconds) {
        const auto steps = static_cast<int>(seconds / FIXED_DT);
        for (int i = 0; i < steps; ++i) {
            now += static_cast<double>(FIXED_DT);
            engine.advance();
        }
    }

    /** @brief Spawns the ship of a player, as the room does. */
    ecs::Entity spawnShip(const std::uint8_t playerIndex, const float x, const float y) {
        const ecs::Entity ship = registry().spawnEntity();
        registry().addComponent(ship, ecs::Transform{.x = x, .y = y});
        registry().addComponent(ship, game::EntityNetState{.subtype = playerIndex});
        ecs::net::replicate(registry(), ship, game::KIND_PLAYER_SHIP);
        return ship;
    }

    /** @brief Spawns a motionless enemy. */
    ecs::Entity spawnEnemy(const float x, const float y) {
        const ecs::Entity enemy = registry().spawnEntity();
        ::prefabs::buildAlienScout(enemy, registry(), {{"x", x}, {"y", y}, {"speed", 0.0F}});
        ecs::net::replicate(registry(), enemy, game::KIND_ALIEN_SCOUT);
        return enemy;
    }

    /** @brief Applies an input holding the shoot button to a ship, as the replication plugin does. */
    void shoot(const ecs::Entity ship) {
        engine.eventBus().publish(ecs::net::InputApplied{
            .entity = ship,
            .input = {.sequence = 1, .held = {game::ACTION_SHOOT}, .axes = {}},
            .deltaTime = FIXED_DT,
        });
    }

    /** @brief Replicated entities of a kind. */
    [[nodiscard]] std::vector<ecs::Entity> entitiesOf(const std::uint32_t kind) {
        std::vector<ecs::Entity> found;
        registry().runSystem<ecs::Replicated>([&](const ecs::Entity entity, const ecs::Replicated& replicated) {
            if (replicated.kind == kind) {
                found.push_back(entity);
            }
        });
        return found;
    }

    double now = 0.0;
    engine::Engine engine;
};

/** @brief Same room, without any level. */
class GameplayWithoutLevelTest : public GameplayTest {
  protected:
    GameplayWithoutLevelTest() : GameplayTest(nullptr) {}
};

}  // namespace

TEST_F(GameplayTest, LevelSpawnsEnemiesAtTheirScrollDistance) {
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
    run(0.5F);
    EXPECT_TRUE(entitiesOf(game::KIND_ALIEN_SCOUT).empty());

    run(0.6F);
    const auto enemies = entitiesOf(game::KIND_ALIEN_SCOUT);
    ASSERT_EQ(enemies.size(), 1U);
    EXPECT_TRUE(registry().hasComponent<ecs::RigidBody2D>(enemies.front()));
    EXPECT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(enemies.front()).y, 160.0F);

    run(1.0F);
    EXPECT_EQ(entitiesOf(game::KIND_ALIEN_SCOUT).size(), 2U);
}

TEST_F(GameplayTest, ScrollEventChangesTheSpeed) {
    run(2.9F);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
    run(0.2F);
    EXPECT_FLOAT_EQ(player().speed(), SLOW_SPEED);
}

TEST_F(GameplayTest, EndOfLevelRestartsIt) {
    // Ends at 5 s (180 px at 60 px/s, then 60 px at 30 px/s): the speed is back to the start and the waves replay.
    run(5.1F);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
    EXPECT_EQ(entitiesOf(game::KIND_ALIEN_SCOUT).size(), 2U);

    run(1.0F);
    EXPECT_EQ(entitiesOf(game::KIND_ALIEN_SCOUT).size(), 3U);
}

TEST_F(GameplayTest, EnemiesThatLeaveTheScreenAreDestroyed) {
    // From x = 1328 at 150 px/s, an enemy is past the left edge after about 9.3 s. In 15 s the level spawns at 1, 2, 6,
    // 7, 11 and 12 s, so the first ones are gone. Entity ids are recycled, so the test looks at positions.
    run(15.0F);
    const auto enemies = entitiesOf(game::KIND_ALIEN_SCOUT);
    EXPECT_LT(enemies.size(), 6U);
    for (const ecs::Entity enemy : enemies) {
        EXPECT_GE(registry().getComponents<ecs::Transform>().get(enemy).x, -64.0F);
    }
}

TEST_F(GameplayWithoutLevelTest, NoLevelSpawnsNoEnemy) {
    run(5.0F);
    EXPECT_TRUE(entitiesOf(game::KIND_ALIEN_SCOUT).empty());
}

TEST(GameplayPluginTest, OnlyKnownPrefabsCanBeSpawned) {
    EXPECT_TRUE(GameplayPlugin::canSpawn("alien_scout"));
    EXPECT_TRUE(GameplayPlugin::canSpawn("boss_core"));
    EXPECT_FALSE(GameplayPlugin::canSpawn("dragon"));
    EXPECT_FALSE(GameplayPlugin::canSpawn(""));
}

TEST_F(GameplayTest, ShootingSpawnsABulletTaggedWithItsShooter) {
    const ecs::Entity ship = spawnShip(1, 200.0F, LANE_Y);
    shoot(ship);
    run(FIXED_DT * 2);

    const auto bullets = entitiesOf(game::KIND_PLAYER_BULLET);
    ASSERT_EQ(bullets.size(), 1U);
    EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(bullets.front()).subtype, 1);
    EXPECT_GT(registry().getComponents<ecs::Transform>().get(bullets.front()).x, 200.0F);
}

TEST_F(GameplayTest, HoldingShootFiresAtTheCooldownRate) {
    const ecs::Entity ship = spawnShip(0, 200.0F, LANE_Y);
    shoot(ship);
    shoot(ship);
    run(FIXED_DT * 2);
    EXPECT_EQ(entitiesOf(game::KIND_PLAYER_BULLET).size(), 1U);

    run(rtype::server::gameplay::SHOT_COOLDOWN);
    shoot(ship);
    run(FIXED_DT * 2);
    EXPECT_EQ(entitiesOf(game::KIND_PLAYER_BULLET).size(), 2U);
}

TEST_F(GameplayTest, KillingAnEnemyScoresForTheShooterOnly) {
    const ecs::Entity ship = spawnShip(1, 200.0F, LANE_Y);
    const ecs::Entity enemy = spawnEnemy(500.0F, LANE_Y);
    shoot(ship);
    run(0.5F);

    EXPECT_FALSE(registry().isAlive(enemy));
    EXPECT_TRUE(entitiesOf(game::KIND_PLAYER_BULLET).empty());
    EXPECT_EQ(gameplay().score(1), POINTS_PER_KILL);
    EXPECT_EQ(gameplay().score(0), 0U);
}

TEST_F(GameplayTest, EachPlayerHasItsOwnScore) {
    const ecs::Entity first = spawnShip(0, 200.0F, LANE_Y);
    const ecs::Entity second = spawnShip(1, 200.0F, LANE_Y + 600.0F);
    spawnEnemy(500.0F, LANE_Y);
    spawnEnemy(700.0F, LANE_Y);
    spawnEnemy(500.0F, LANE_Y + 600.0F);

    shoot(first);
    shoot(second);
    run(0.5F);
    shoot(first);
    run(1.0F);

    EXPECT_EQ(gameplay().score(0), 2 * POINTS_PER_KILL);
    EXPECT_EQ(gameplay().score(1), POINTS_PER_KILL);
    EXPECT_EQ(gameplay().score(5), 0U);
}

TEST_F(GameplayTest, ShipWithoutNetStateCannotShoot) {
    const ecs::Entity entity = registry().spawnEntity();
    registry().addComponent(entity, ecs::Transform{.x = 200.0F, .y = LANE_Y});
    shoot(entity);
    run(FIXED_DT * 2);
    EXPECT_TRUE(entitiesOf(game::KIND_PLAYER_BULLET).empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
