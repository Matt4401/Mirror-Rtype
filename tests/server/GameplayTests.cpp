#include <gtest/gtest.h>

#include <cstdint>
#include <string>
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
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "protocol/Messages.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace {

using rtype::server::gameplay::ENEMY_SPAWN_INTERVAL;
using rtype::server::gameplay::GameplayPlugin;
using rtype::server::gameplay::INTERMISSION_DURATION;
using rtype::server::gameplay::LevelInfo;
using rtype::server::gameplay::LevelProgress;
using rtype::server::gameplay::POINTS_PER_KILL;
using rtype::server::gameplay::SHIP_SPAWN_FIRST_Y;
using rtype::server::gameplay::SHIP_SPAWN_SPACING_Y;
using rtype::server::gameplay::SHIP_SPAWN_X;
namespace protocol = rtype::protocol;
namespace game = rtype::game;

constexpr float FIXED_DT = 1.0F / 60.0F;
/** @brief Height far from every enemy spawn height, so that the waves never get in the way. */
constexpr float LANE_Y = 40.0F;

/** @brief Room-like engine with a fake clock: physics and the R-Type rules, nothing else. */
class GameplayTest : public ::testing::Test {
  protected:
    GameplayTest() : engine([this] { return now; }) {
        engine.setFixedDeltaTime(FIXED_DT);
        engine.addPlugin<systems::PhysicsPlugin>();
        engine.addPlugin<GameplayPlugin>();
        engine.init();
        gameplay().setPlayerCount(2);
    }

    [[nodiscard]] GameplayPlugin& gameplay() { return engine.getPlugin<GameplayPlugin>(); }
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

}  // namespace

TEST_F(GameplayTest, EnemiesSpawnAtRegularIntervals) {
    run(ENEMY_SPAWN_INTERVAL * 0.5F);
    EXPECT_TRUE(entitiesOf(game::KIND_ALIEN_SCOUT).empty());

    run(ENEMY_SPAWN_INTERVAL);
    const auto enemies = entitiesOf(game::KIND_ALIEN_SCOUT);
    ASSERT_EQ(enemies.size(), 1U);
    EXPECT_TRUE(registry().hasComponent<ecs::RigidBody2D>(enemies.front()));

    run(ENEMY_SPAWN_INTERVAL);
    EXPECT_EQ(entitiesOf(game::KIND_ALIEN_SCOUT).size(), 2U);
}

TEST_F(GameplayTest, EnemiesThatLeaveTheScreenAreDestroyed) {
    // From x = 1328 at 150 px/s, an enemy is past the left edge after about 9.3 s: in 15 s, 12 enemies spawn and the
    // first ones are gone. Entity ids are recycled, so the test looks at positions, not at the first enemy's id.
    run(15.0F);
    const auto enemies = entitiesOf(game::KIND_ALIEN_SCOUT);
    EXPECT_LT(enemies.size(), 12U);
    for (const ecs::Entity enemy : enemies) {
        EXPECT_GE(registry().getComponents<ecs::Transform>().get(enemy).x, -64.0F);
    }
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

/** @brief Gameplay engine recording the level messages the room would send. */
class LevelFlowTest : public GameplayTest {
  protected:
    LevelFlowTest() {
        auto& bus = engine.eventBus();
        bus.subscribe<protocol::LevelStart>([this](const protocol::LevelStart& message) {
            log.push_back("start " + std::to_string(message.levelIndex) + "/" + std::to_string(message.levelCount) +
                          " " + message.name);
        });
        bus.subscribe<protocol::LevelComplete>([this](const protocol::LevelComplete& message) {
            log.push_back("complete " + std::to_string(message.levelIndex));
        });
        bus.subscribe<protocol::GameOver>([this](const protocol::GameOver& message) {
            log.push_back(message.reason == protocol::GameOverReason::Victory ? "victory" : "defeat");
        });
        // Two short levels that end after one second, whatever the kills.
        gameplay().setLevels({LevelInfo{.name = "A", .spawnInterval = 0.3F}, LevelInfo{.name = "B"}});
        gameplay().setLevelFinished(
            [](const LevelProgress& progress, const LevelInfo& /*level*/) { return progress.elapsed >= 1.0F; });
    }

    std::vector<std::string> log;
};

TEST_F(LevelFlowTest, StartGameAnnouncesTheFirstLevel) {
    gameplay().startGame();
    EXPECT_EQ(log, (std::vector<std::string>{"start 0/2 A"}));
    EXPECT_EQ(gameplay().levelIndex(), 0U);
}

TEST_F(LevelFlowTest, ClearingALevelRemovesEnemiesAndRepositionsShips) {
    const ecs::Entity ship = spawnShip(1, 900.0F, LANE_Y);
    gameplay().startGame();
    run(0.95F);
    EXPECT_FALSE(entitiesOf(game::KIND_ALIEN_SCOUT).empty());

    run(0.1F);
    EXPECT_EQ(log, (std::vector<std::string>{"start 0/2 A", "complete 0"}));
    EXPECT_TRUE(entitiesOf(game::KIND_ALIEN_SCOUT).empty());
    const auto& transform = registry().getComponents<ecs::Transform>().get(ship);
    EXPECT_FLOAT_EQ(transform.x, SHIP_SPAWN_X);
    EXPECT_FLOAT_EQ(transform.y, SHIP_SPAWN_FIRST_Y + SHIP_SPAWN_SPACING_Y);

    run(INTERMISSION_DURATION * 0.5F);
    EXPECT_TRUE(entitiesOf(game::KIND_ALIEN_SCOUT).empty()) << "nothing spawns between two levels";
    run(INTERMISSION_DURATION * 0.6F);
    EXPECT_EQ(log.back(), "start 1/2 B");
}

TEST_F(LevelFlowTest, ClearingTheLastLevelIsAVictoryAndStopsTheWaves) {
    spawnShip(0, 200.0F, LANE_Y);
    gameplay().startGame();
    run(1.1F + INTERMISSION_DURATION + 1.1F);
    EXPECT_EQ(log, (std::vector<std::string>{"start 0/2 A", "complete 0", "start 1/2 B", "complete 1", "victory"}));
    EXPECT_TRUE(gameplay().isOver());

    run(5.0F);
    EXPECT_TRUE(entitiesOf(game::KIND_ALIEN_SCOUT).empty());
    EXPECT_EQ(log.size(), 5U);
}

TEST_F(LevelFlowTest, LosingEveryShipEndsTheGame) {
    const ecs::Entity ship = spawnShip(0, 200.0F, LANE_Y);
    gameplay().startGame();
    run(0.2F);
    registry().killEntity(ship);
    run(0.1F);
    EXPECT_EQ(log.back(), "defeat");
    EXPECT_TRUE(gameplay().isOver());
}

TEST_F(GameplayTest, DefaultRuleClearsALevelAtItsKillQuota) {
    std::vector<std::string> log;
    engine.eventBus().subscribe<protocol::GameOver>([&](const protocol::GameOver& message) {
        log.push_back(message.reason == protocol::GameOverReason::Victory ? "victory" : "defeat");
        EXPECT_EQ(message.score, POINTS_PER_KILL);
        EXPECT_EQ(message.playerScores, (std::vector<std::uint32_t>{0, POINTS_PER_KILL}));
    });
    gameplay().setLevels({LevelInfo{.name = "Only", .killQuota = 1, .maxDuration = 0.0F, .spawnInterval = 99.0F}});
    gameplay().setLevelFinished({});
    const ecs::Entity ship = spawnShip(1, 200.0F, LANE_Y);
    gameplay().startGame();
    spawnEnemy(500.0F, LANE_Y);
    shoot(ship);
    run(0.5F);
    EXPECT_EQ(log, (std::vector<std::string>{"victory"}));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
