#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "components/Replicated.hpp"
#include "components/RigidBody2D.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "gameplay/BossFightPlugin.hpp"
#include "gameplay/CombatPlugin.hpp"
#include "gameplay/Gameplay.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "gameplay/LevelDirectorPlugin.hpp"
#include "gameplay/RespawnPlugin.hpp"
#include "gameplay/ScriptBindings.hpp"
#include "gameplay/ScriptPrefabs.hpp"
#include "level/Level.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/player_ship/PlayerShip.hpp"
#include "prefabs/power_up/PowerUp.hpp"
#include "protocol/Messages.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/script/ScriptSystem.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace {

using rtype::server::gameplay::CombatPlugin;
using rtype::server::gameplay::LevelDirectorPlugin;
using rtype::server::gameplay::POINTS_PER_KILL;
using rtype::server::gameplay::RAPID_FIRE_DURATION;
using rtype::server::gameplay::RESPAWN_DURATION;
using rtype::server::gameplay::shipSpawnPoint;
using rtype::server::gameplay::ShipSpawnPoint;
namespace game = rtype::game;
namespace protocol = rtype::protocol;
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
        rtype::server::gameplay::addGameplayPlugins(engine, played);
        engine.init();
        combat().setPlayerCount(2);
    }

    [[nodiscard]] CombatPlugin& combat() { return engine.getPlugin<CombatPlugin>(); }
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

    /** @brief Spawns the ship of a player, as the room does: with the collider of the `player_ship` prefab. */
    ecs::Entity spawnShip(const std::uint8_t playerIndex, const float x, const float y) {
        const ecs::Entity ship = registry().spawnEntity();
        ::prefabs::buildPlayerShip(ship, registry(), {{"x", x}, {"y", y}});
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

/** @brief Health of the test boss: a few shots. */
constexpr std::int64_t BOSS_HEALTH = 3;
/** @brief Variant of the test boss, sent as its subtype. */
constexpr std::int64_t BOSS_VARIANT = 2;
/** @brief Height of the test boss, and of the ship fighting it. */
constexpr float BOSS_Y = 360.0F;

/** @brief Level with a boss at 1 s, driven by `scripts/Boss.wren`, then an end far away. */
std::shared_ptr<const level::Level> bossLevel() {
    auto built = std::make_shared<level::Level>();
    built->scrollSpeed = LEVEL_SPEED;
    built->timeline.add({.position = 60.0F,
                         .type = std::string(level::EVENT_BOSS),
                         .params = {{"prefab", std::string("boss_core")},
                                    {"x", 1328.0},
                                    {"y", static_cast<double>(BOSS_Y)},
                                    {"health", BOSS_HEALTH},
                                    {"variant", BOSS_VARIANT},
                                    {"script", std::string("scripts/Boss.wren")}}});
    built->timeline.add({.position = 6000.0F, .type = std::string(level::EVENT_END), .params = {}});
    return built;
}

/** @brief Room playing `bossLevel()`; no ScriptPlugin, so the script is attached but never run. */
class BossFightTest : public GameplayTest {
  protected:
    BossFightTest() : GameplayTest(bossLevel()) {}

    /** @brief The boss, if alive. */
    [[nodiscard]] std::vector<ecs::Entity> bosses() { return entitiesOf(game::KIND_BOSS_CORE); }
};

/** @brief Height of the test terrain. */
constexpr float TERRAIN_HEIGHT = 120.0F;
/** @brief Length of the test terrain: 3 blocks of 256 px, the last one past the section end. */
constexpr float TERRAIN_LENGTH = 600.0F;

/** @brief Level with a bottom terrain section at 1 s, then an end far away. */
std::shared_ptr<const level::Level> terrainLevel() {
    auto built = std::make_shared<level::Level>();
    built->scrollSpeed = LEVEL_SPEED;
    built->timeline.add({.position = 60.0F,
                         .type = std::string(level::EVENT_TERRAIN),
                         .params = {{"side", std::string(level::TERRAIN_BOTTOM)},
                                    {"height", static_cast<double>(TERRAIN_HEIGHT)},
                                    {"length", static_cast<double>(TERRAIN_LENGTH)}}});
    built->timeline.add({.position = 6000.0F, .type = std::string(level::EVENT_END), .params = {}});
    return built;
}

/** @brief Room playing `terrainLevel()`. */
class TerrainTest : public GameplayTest {
  protected:
    TerrainTest() : GameplayTest(terrainLevel()) {}

    /** @brief The terrain columns on the screen or entering it. */
    [[nodiscard]] std::vector<ecs::Entity> columns() { return entitiesOf(game::KIND_OBSTACLE); }
};

/** @brief Bottom terrain at 1 s, then a boss at 2.5 s, while the terrain is on the screen. */
std::shared_ptr<const level::Level> terrainThenBossLevel() {
    auto built = std::make_shared<level::Level>();
    built->scrollSpeed = LEVEL_SPEED;
    built->timeline.add({.position = 60.0F,
                         .type = std::string(level::EVENT_TERRAIN),
                         .params = {{"side", std::string(level::TERRAIN_BOTTOM)},
                                    {"height", static_cast<double>(TERRAIN_HEIGHT)},
                                    {"length", static_cast<double>(TERRAIN_LENGTH)}}});
    built->timeline.add(
        {.position = 150.0F, .type = std::string(level::EVENT_BOSS), .params = {{"prefab", std::string("boss_core")}}});
    built->timeline.add({.position = 6000.0F, .type = std::string(level::EVENT_END), .params = {}});
    return built;
}

/** @brief Room playing a level with terrain (`terrainLevel()` by default), where ships crash. */
class RespawnTest : public GameplayTest {
  protected:
    explicit RespawnTest(std::shared_ptr<const level::Level> played = terrainLevel())
        : GameplayTest(std::move(played)) {}

    /** @brief The terrain columns on the screen or entering it. */
    [[nodiscard]] std::vector<ecs::Entity> columns() { return entitiesOf(game::KIND_OBSTACLE); }

    /** @brief Puts the ship on the first terrain column, then runs until it is destroyed (at most 1 s). */
    void crash(const ecs::Entity ship) {
        const auto built = columns();
        ASSERT_FALSE(built.empty());
        const ecs::Transform column = registry().getComponents<ecs::Transform>().get(built.front());
        auto& transform = registry().getComponents<ecs::Transform>().get(ship);
        transform.x = column.x;
        transform.y = column.y;
        for (int step = 0; step < 60 && !invulnerable(ship); ++step) {
            run(FIXED_DT);
        }
        ASSERT_TRUE(invulnerable(ship));
    }

    /** @brief Whether the ship carries the invulnerable flag. */
    [[nodiscard]] bool invulnerable(const ecs::Entity ship) {
        return (registry().getComponents<game::EntityNetState>().get(ship).flags & game::FLAG_INVULNERABLE) != 0;
    }
};

/** @brief Room playing `terrainThenBossLevel()`. */
class RespawnDuringBossTest : public RespawnTest {
  protected:
    RespawnDuringBossTest() : RespawnTest(terrainThenBossLevel()) {}
};

/** @brief Boss event at 1 s, followed in the same step by a `scroll` and an `end`. */
std::shared_ptr<const level::Level> bossThenEndLevel() {
    auto built = std::make_shared<level::Level>();
    built->scrollSpeed = LEVEL_SPEED;
    built->timeline.add(
        {.position = 60.0F, .type = std::string(level::EVENT_BOSS), .params = {{"prefab", std::string("boss_core")}}});
    built->timeline.add({.position = 60.0F,
                         .type = std::string(level::EVENT_SCROLL),
                         .params = {{"speed", static_cast<double>(SLOW_SPEED)}}});
    built->timeline.add({.position = 60.0F, .type = std::string(level::EVENT_END), .params = {}});
    return built;
}

/** @brief Room playing `bossThenEndLevel()`. */
class BossThenEndTest : public GameplayTest {
  protected:
    BossThenEndTest() : GameplayTest(bossThenEndLevel()) {}

    /** @brief The bosses alive. */
    [[nodiscard]] std::vector<ecs::Entity> bosses() { return entitiesOf(game::KIND_BOSS_CORE); }
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

TEST_F(GameplayTest, EndOfLevelRestartsItBeforeTheGameStarts) {
    // Ends at 5 s (180 px at 60 px/s, then 60 px at 30 px/s): the speed is back to the start and the waves replay.
    run(5.1F);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
    EXPECT_EQ(entitiesOf(game::KIND_ALIEN_SCOUT).size(), 2U);

    run(1.0F);
    EXPECT_EQ(entitiesOf(game::KIND_ALIEN_SCOUT).size(), 3U);
}

/** @brief Gameplay engine recording the session messages the room would send. */
class GameFlowTest : public GameplayTest {
  protected:
    GameFlowTest() {
        ecs::EventBus& bus = engine.eventBus();
        bus.subscribe<protocol::LevelStart>([this](const protocol::LevelStart& message) {
            log.push_back("start " + std::to_string(message.levelIndex) + "/" + std::to_string(message.levelCount) +
                          " " + message.name);
        });
        bus.subscribe<protocol::LevelComplete>([this](const protocol::LevelComplete& message) {
            log.push_back("complete " + std::to_string(message.levelIndex));
            complete = message;
        });
        bus.subscribe<protocol::GameOver>([this](const protocol::GameOver& message) {
            log.push_back(message.reason == protocol::GameOverReason::Victory ? "victory" : "other");
            over = message;
        });
    }

    std::vector<std::string> log;
    std::optional<protocol::LevelComplete> complete;
    std::optional<protocol::GameOver> over;
};

TEST_F(GameFlowTest, StartGameAnnouncesTheLevel) {
    engine.getPlugin<LevelDirectorPlugin>().startGame();
    EXPECT_EQ(log, (std::vector<std::string>{"start 0/1 Test"}));
}

TEST_F(GameFlowTest, EndOfLevelWinsTheGameWithTheScores) {
    const ecs::Entity ship = spawnShip(1, 200.0F, LANE_Y);
    engine.getPlugin<LevelDirectorPlugin>().startGame();
    spawnEnemy(500.0F, LANE_Y);
    shoot(ship);
    // The level ends at 5 s.
    run(5.1F);
    EXPECT_EQ(log, (std::vector<std::string>{"start 0/1 Test", "complete 0", "victory"}));
    ASSERT_TRUE(complete.has_value());
    ASSERT_TRUE(over.has_value());
    EXPECT_EQ(over->score, POINTS_PER_KILL);
    EXPECT_EQ(over->playerScores, (std::vector<std::uint32_t>{0, POINTS_PER_KILL}));
    EXPECT_EQ(complete->playerScores, over->playerScores);
}

TEST_F(GameFlowTest, NothingIsPlayedAfterTheGameIsWon) {
    engine.getPlugin<LevelDirectorPlugin>().startGame();
    run(5.1F);
    EXPECT_FLOAT_EQ(player().speed(), 0.0F);
    const std::size_t enemies = entitiesOf(game::KIND_ALIEN_SCOUT).size();
    run(3.0F);
    EXPECT_LE(entitiesOf(game::KIND_ALIEN_SCOUT).size(), enemies) << "the level must not restart";
    EXPECT_EQ(log.size(), 3U);
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

TEST_F(GameplayWithoutLevelTest, MissileHittingAShipDestroysIt) {
    const ecs::Entity ship = spawnShip(1, 600.0F, 360.0F);
    const ecs::Entity missile = registry().spawnEntity();
    ::prefabs::buildMissile(missile, registry(), {{"x", 700.0F}, {"y", 360.0F}});
    ecs::net::replicate(registry(), missile, game::KIND_MISSILE);
    run(0.5F);

    EXPECT_FALSE(registry().isAlive(missile));
    ASSERT_TRUE(registry().isAlive(ship));
    EXPECT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(ship).x, shipSpawnPoint(1).x);
    EXPECT_NE(registry().getComponents<game::EntityNetState>().get(ship).flags & game::FLAG_INVULNERABLE, 0);
}

TEST_F(GameplayWithoutLevelTest, MissileGoesThroughARespawningShip) {
    const ShipSpawnPoint spawn = shipSpawnPoint(0);
    const ecs::Entity ship = spawnShip(0, 600.0F, spawn.y);
    const ecs::Entity first = registry().spawnEntity();
    ::prefabs::buildMissile(first, registry(), {{"x", 700.0F}, {"y", spawn.y}});
    run(0.5F);
    ASSERT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(ship).x, spawn.x);

    // Fired at the respawned ship: it flies through.
    const ecs::Entity second = registry().spawnEntity();
    ::prefabs::buildMissile(second, registry(), {{"x", spawn.x + 100.0F}, {"y", spawn.y}});
    run(0.5F);
    EXPECT_TRUE(registry().isAlive(second));
    EXPECT_LT(registry().getComponents<ecs::Transform>().get(second).x, spawn.x);
}

TEST_F(GameplayWithoutLevelTest, EnemyTouchingAShipDestroysIt) {
    const ecs::Entity ship = spawnShip(0, 600.0F, 360.0F);
    const ecs::Entity enemy = spawnEnemy(600.0F, 360.0F);
    run(0.2F);

    EXPECT_TRUE(registry().isAlive(enemy));
    EXPECT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(ship).x, shipSpawnPoint(0).x);
    EXPECT_NE(registry().getComponents<game::EntityNetState>().get(ship).flags & game::FLAG_INVULNERABLE, 0);
}

TEST_F(GameplayWithoutLevelTest, PowerUpGivesRapidFire) {
    const ecs::Entity ship = spawnShip(0, 100.0F, 360.0F);
    // Holds the shoot button for 0.5 s, then counts the bullets: none has left the screen yet (900 px/s from x = 100).
    // Waits first for the bullets of the previous burst to leave.
    const auto burst = [&] {
        run(1.6F);
        for (int step = 0; step < 30; ++step) {
            shoot(ship);
            run(FIXED_DT);
        }
        return entitiesOf(game::KIND_PLAYER_BULLET).size();
    };
    // One shot every 0.2 s: at 0, 0.2 and 0.4 s.
    EXPECT_EQ(burst(), 3U);

    const ecs::Entity powerUp = registry().spawnEntity();
    ::prefabs::buildPowerUp(powerUp, registry(), {{"x", 100.0F}, {"y", 360.0F}, {"speed", 0.0F}});
    run(0.1F);
    EXPECT_FALSE(registry().isAlive(powerUp));
    // One shot every 0.08 s.
    EXPECT_GE(burst(), 6U);

    // The bonus wears off.
    run(RAPID_FIRE_DURATION);
    EXPECT_EQ(burst(), 3U);
}

TEST_F(BossFightTest, BossEventStopsTheScrollAndSpawnsTheBoss) {
    run(0.9F);
    EXPECT_TRUE(bosses().empty());
    run(0.2F);

    EXPECT_FLOAT_EQ(player().speed(), 0.0F);
    const auto spawned = bosses();
    ASSERT_EQ(spawned.size(), 1U);
    EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(spawned.front()).health, BOSS_HEALTH);
    EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(spawned.front()).subtype, BOSS_VARIANT);
    EXPECT_EQ(registry().getComponents<prefabs::Health>().get(spawned.front()).points, BOSS_HEALTH);
}

TEST_F(BossFightTest, BossGetsTheScriptOfItsEvent) {
    run(1.1F);
    const auto spawned = bosses();
    ASSERT_EQ(spawned.size(), 1U);
    ASSERT_TRUE(registry().hasComponent<ecs::ScriptComponent>(spawned.front()));
    const auto& script = registry().getComponents<ecs::ScriptComponent>().get(spawned.front());
    EXPECT_EQ(script.moduleName, "scripts/Boss");
    EXPECT_EQ(script.className, "Boss");
    EXPECT_TRUE(script.hasUpdate);
}

TEST_F(BossFightTest, BossFliesInAndStopsAtItsPlace) {
    // From x = 1328 at 120 px/s, the boss reaches stop_x (1000) after about 2.7 s.
    run(5.0F);
    const auto spawned = bosses();
    ASSERT_EQ(spawned.size(), 1U);
    const float stoppedAt = registry().getComponents<ecs::Transform>().get(spawned.front()).x;
    EXPECT_LE(stoppedAt, prefabs::DEFAULT_BOSS_CORE_STOP_X);
    EXPECT_GT(stoppedAt, prefabs::DEFAULT_BOSS_CORE_STOP_X - 10.0F);

    run(2.0F);
    EXPECT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(spawned.front()).x, stoppedAt);
}

TEST_F(BossFightTest, ShotsWearTheBossDownThenTheScrollResumes) {
    run(4.0F);
    const ecs::Entity ship = spawnShip(0, 200.0F, BOSS_Y);
    const auto spawned = bosses();
    ASSERT_EQ(spawned.size(), 1U);
    const ecs::Entity boss = spawned.front();

    std::int64_t lowestHealth = BOSS_HEALTH;
    for (int volley = 0; volley < 40 && registry().isAlive(boss); ++volley) {
        shoot(ship);
        run(0.25F);
        if (registry().isAlive(boss)) {
            lowestHealth =
                std::min<std::int64_t>(lowestHealth, registry().getComponents<prefabs::Health>().get(boss).points);
            EXPECT_EQ(combat().score(0), 0U) << "only the kill scores";
        }
    }

    EXPECT_FALSE(registry().isAlive(boss));
    EXPECT_LT(lowestHealth, BOSS_HEALTH);
    EXPECT_EQ(combat().score(0), POINTS_PER_KILL);
    run(FIXED_DT * 2);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
}

TEST_F(BossThenEndTest, EventsInTheSameStepAsTheBossWaitForTheFight) {
    run(1.1F);
    ASSERT_EQ(bosses().size(), 1U);
    EXPECT_FLOAT_EQ(player().speed(), 0.0F) << "the scroll event must not restart the scroll mid-fight";

    run(5.0F);
    EXPECT_EQ(bosses().size(), 1U) << "the end event must not restart the level while the boss is alive";
    EXPECT_FLOAT_EQ(player().speed(), 0.0F);
}

TEST_F(BossThenEndTest, WaitingEventsPlayOnceTheBossIsDestroyed) {
    run(1.1F);
    const auto spawned = bosses();
    ASSERT_EQ(spawned.size(), 1U);
    registry().killEntity(spawned.front());

    // The scroll event sets the slow speed, then the end restarts the level at its initial speed.
    run(FIXED_DT * 2);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
    EXPECT_TRUE(bosses().empty());

    // The restarted level reaches its boss again: one boss, not two.
    run(1.1F);
    EXPECT_EQ(bosses().size(), 1U);
    EXPECT_FLOAT_EQ(player().speed(), 0.0F);
}

TEST_F(TerrainTest, TerrainEntersFromTheRightColumnByColumn) {
    run(1.1F);
    const auto entering = columns();
    ASSERT_FALSE(entering.empty());
    EXPECT_LT(entering.size(), 3U) << "blocks are built as they enter the screen";
    for (const ecs::Entity column : entering) {
        const auto& transform = registry().getComponents<ecs::Transform>().get(column);
        EXPECT_GT(transform.x, 1280.0F);
        EXPECT_FLOAT_EQ(transform.y, 720.0F - (TERRAIN_HEIGHT / 2.0F));
        EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(column).subtype,
                  TERRAIN_HEIGHT / game::TERRAIN_HEIGHT_UNIT);
    }

    // A block enters every 256 / 60 = 4.3 s.
    run(8.0F);
    EXPECT_EQ(columns().size(), 3U);
}

TEST_F(TerrainTest, TerrainScrollsAtTheScrollSpeed) {
    run(1.1F);
    const auto entering = columns();
    ASSERT_FALSE(entering.empty());
    const float before = registry().getComponents<ecs::Transform>().get(entering.front()).x;
    run(1.0F);
    const float after = registry().getComponents<ecs::Transform>().get(entering.front()).x;
    EXPECT_NEAR(before - after, LEVEL_SPEED, 2.0F);
}

TEST_F(TerrainTest, ShipTouchingTheTerrainRespawnsAtItsStart) {
    const ecs::Entity grounded = spawnShip(1, 1200.0F, 700.0F);
    const ecs::Entity flying = spawnShip(0, 1200.0F, LANE_Y);
    const auto& transforms = registry().getComponents<ecs::Transform>();
    for (int step = 0; step < 400 && transforms.get(grounded).x == 1200.0F; ++step) {
        run(FIXED_DT);
    }

    ASSERT_TRUE(registry().isAlive(grounded));
    const ShipSpawnPoint spawn = shipSpawnPoint(1);
    EXPECT_FLOAT_EQ(transforms.get(grounded).x, spawn.x);
    EXPECT_FLOAT_EQ(transforms.get(grounded).y, spawn.y);
    EXPECT_NE(registry().getComponents<game::EntityNetState>().get(grounded).flags & game::FLAG_INVULNERABLE, 0);
    EXPECT_FLOAT_EQ(transforms.get(flying).x, 1200.0F);
    EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(flying).flags & game::FLAG_INVULNERABLE, 0);
}

TEST_F(RespawnTest, RespawnedShipCannotBeDestroyedForItsDuration) {
    run(1.5F);
    const ecs::Entity ship = spawnShip(0, 200.0F, LANE_Y);
    crash(ship);
    const ShipSpawnPoint spawn = shipSpawnPoint(0);
    ASSERT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(ship).x, spawn.x);

    // Back on the terrain while invulnerable: nothing happens.
    const ecs::Transform column = registry().getComponents<ecs::Transform>().get(columns().front());
    auto& transform = registry().getComponents<ecs::Transform>().get(ship);
    transform.x = column.x;
    transform.y = column.y;
    run(RESPAWN_DURATION - 0.5F);
    EXPECT_FLOAT_EQ(transform.x, column.x);
    EXPECT_TRUE(invulnerable(ship));

    // Once the respawn is over, the ship is vulnerable again.
    transform.y = LANE_Y;
    run(1.0F);
    EXPECT_FALSE(invulnerable(ship));
}

TEST_F(RespawnTest, ShipStillInTheTerrainWhenItsRespawnEndsIsDestroyedAgain) {
    run(1.5F);
    const ecs::Entity ship = spawnShip(0, 200.0F, LANE_Y);
    crash(ship);

    // Parked in the terrain for the whole respawn: its contact began while it could not be hit.
    const ecs::Transform block = registry().getComponents<ecs::Transform>().get(columns().front());
    auto& transform = registry().getComponents<ecs::Transform>().get(ship);
    transform.x = block.x;
    transform.y = block.y;
    run(RESPAWN_DURATION + 0.2F);

    EXPECT_TRUE(invulnerable(ship));
    EXPECT_FLOAT_EQ(transform.x, shipSpawnPoint(0).x);
}

TEST_F(RespawnTest, ScrollWaitsDuringTheRespawn) {
    run(1.5F);
    const ecs::Entity ship = spawnShip(0, 200.0F, LANE_Y);
    crash(ship);
    EXPECT_FLOAT_EQ(player().speed(), 0.0F);

    // The columns take the new speed on the next step.
    run(FIXED_DT);
    const ecs::Entity column = columns().front();
    const float before = registry().getComponents<ecs::Transform>().get(column).x;
    run(RESPAWN_DURATION - 0.5F);
    EXPECT_FLOAT_EQ(player().speed(), 0.0F);
    EXPECT_FLOAT_EQ(registry().getComponents<ecs::Transform>().get(column).x, before) << "the terrain waits too";

    run(1.0F);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
}

TEST_F(RespawnDuringBossTest, RespawnAndBossFightBothHoldTheScroll) {
    run(2.6F);
    const auto bosses = entitiesOf(game::KIND_BOSS_CORE);
    ASSERT_EQ(bosses.size(), 1U);
    const ecs::Entity ship = spawnShip(0, 200.0F, LANE_Y);
    crash(ship);

    // The boss dies first: the respawn still holds the scroll.
    registry().killEntity(bosses.front());
    run(1.0F);
    EXPECT_FLOAT_EQ(player().speed(), 0.0F);

    run(RESPAWN_DURATION);
    EXPECT_FALSE(invulnerable(ship));
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
}

TEST_F(TerrainTest, TerrainLeavingTheScreenIsDestroyed) {
    // The last block is built at about 8.5 s, its right edge at 1600: past the left edge after (1600 + 64) / 60 = 28 s.
    run(10.0F);
    EXPECT_EQ(columns().size(), 3U);
    run(30.0F);
    EXPECT_TRUE(columns().empty());
}

/** @brief Script of the scripted boss test: spawns a missile at spawn, then holds the boss at y = 200. */
constexpr std::string_view TEST_BOSS_SCRIPT = R"(
import "core/Engine" for Engine
import "core/Position" for Position

class TestBoss {
    construct new(entity) {
        _id = (entity is Num) ? entity : entity.id
        _pos = Position.new(_id)
    }

    onSpawn() {
        Engine.spawnPrefab("missile", _pos.x - 40, _pos.y)
    }

    update(dt) {
        _pos.y = 200
    }
}
)";

/**
 * @brief Room engine with the scripting plugins wired as `Room` does, playing a boss driven by `TEST_BOSS_SCRIPT`.
 *
 * The script is written to a temporary file: the room loads scripts from disk.
 */
class ScriptedBossTest : public ::testing::Test {
  protected:
    /**
     * @param shipped Script of the game to play; none writes `TEST_BOSS_SCRIPT` to a temporary file instead.
     */
    explicit ScriptedBossTest(const std::optional<std::filesystem::path>& shipped = std::nullopt)
        : tempDir(std::filesystem::temp_directory_path() /
                  ("rtype_test_boss_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()))),
          scriptPath(shipped.value_or(tempDir / "TestBoss.wren")),
          engine([this] { return now; }) {
        if (!shipped) {
            std::filesystem::create_directories(tempDir);
            std::ofstream(scriptPath) << TEST_BOSS_SCRIPT;
        }

        auto built = std::make_shared<level::Level>();
        built->scrollSpeed = LEVEL_SPEED;
        built->timeline.add({.position = 0.0F,
                             .type = std::string(level::EVENT_BOSS),
                             .params = {{"prefab", std::string("boss_core")},
                                        {"x", 900.0},
                                        {"y", 360.0},
                                        {"script", scriptPath.generic_string()}}});

        engine.setFixedDeltaTime(FIXED_DT);
        engine.addPlugin<systems::PhysicsPlugin>();
        engine.addPlugin<timeline::TimelinePlugin>();
        rtype::server::gameplay::addGameplayPlugins(engine, built);
        engine.addPlugin<prefabs::PrefabPlugin>();
        engine.addPlugin<scripting::ScriptPlugin>(scripting::ScriptTick::FixedStep);
        engine.init();
        prefabs::PrefabManager& manager = engine.getPlugin<prefabs::PrefabPlugin>().manager();
        rtype::server::gameplay::registerScriptPrefabs(manager);
        systems::ScriptSystem& scripts = engine.getPlugin<scripting::ScriptPlugin>().scriptSystem();
        scripts.setPrefabManager(&manager);
        bound = rtype::server::gameplay::bindScriptComponents(scripts);
    }

    ~ScriptedBossTest() override {
        std::error_code ignored;
        std::filesystem::remove_all(tempDir, ignored);
    }

    ScriptedBossTest(const ScriptedBossTest&) = delete;
    ScriptedBossTest& operator=(const ScriptedBossTest&) = delete;
    ScriptedBossTest(ScriptedBossTest&&) = delete;
    ScriptedBossTest& operator=(ScriptedBossTest&&) = delete;

    /** @brief Runs the engine for some seconds of game time. */
    void run(const float seconds) {
        const auto steps = static_cast<int>(seconds / FIXED_DT);
        for (int i = 0; i < steps; ++i) {
            now += static_cast<double>(FIXED_DT);
            engine.advance();
        }
    }

    /** @brief Replicated entities of a kind. */
    [[nodiscard]] std::vector<ecs::Entity> entitiesOf(const std::uint32_t kind) {
        std::vector<ecs::Entity> found;
        engine.registry().runSystem<ecs::Replicated>([&](const ecs::Entity entity, const ecs::Replicated& replicated) {
            if (replicated.kind == kind) {
                found.push_back(entity);
            }
        });
        return found;
    }

    std::filesystem::path tempDir;
    std::filesystem::path scriptPath;
    double now = 0.0;
    engine::Engine engine;
    bool bound = false;
};

TEST_F(ScriptedBossTest, ScriptMovesItsBossAndItsSpawnsAreReplicated) {
    ASSERT_TRUE(bound);
    run(0.5F);

    const auto bosses = entitiesOf(game::KIND_BOSS_CORE);
    ASSERT_EQ(bosses.size(), 1U);
    EXPECT_FLOAT_EQ(engine.registry().getComponents<ecs::Transform>().get(bosses.front()).y, 200.0F);

    const auto missiles = entitiesOf(rtype::protocol::makeEntityKind("missile"));
    ASSERT_EQ(missiles.size(), 1U);
    EXPECT_TRUE(engine.registry().hasComponent<game::EntityNetState>(missiles.front()));
}

TEST_F(ScriptedBossTest, ScriptsCannotSpawnABoss) {
    const prefabs::PrefabManager& manager = engine.getPlugin<prefabs::PrefabPlugin>().manager();
    EXPECT_TRUE(manager.has("missile"));
    EXPECT_TRUE(manager.has("alien_scout"));
    EXPECT_FALSE(manager.has("boss_core")) << "a boss fight only starts from a level boss event";
}

TEST_F(ScriptedBossTest, ScriptsOnlyRunOnFixedSteps) {
    run(0.2F);
    const auto bosses = entitiesOf(game::KIND_BOSS_CORE);
    ASSERT_EQ(bosses.size(), 1U);
    auto& transform = engine.registry().getComponents<ecs::Transform>().get(bosses.front());
    transform.y = 360.0F;

    // The room loop advances the engine far more often than the simulation rate: no fixed step, no script update.
    for (int i = 0; i < 20; ++i) {
        engine.advance();
    }
    EXPECT_FLOAT_EQ(transform.y, 360.0F);

    run(FIXED_DT * 2);
    EXPECT_FLOAT_EQ(transform.y, 200.0F);
}

TEST_F(ScriptedBossTest, ScriptOfADeadBossDoesNotCrashTheRoom) {
    run(0.2F);
    const auto bosses = entitiesOf(game::KIND_BOSS_CORE);
    ASSERT_EQ(bosses.size(), 1U);
    engine.registry().killEntity(bosses.front());
    run(1.0F);
    EXPECT_TRUE(entitiesOf(game::KIND_BOSS_CORE).empty());
}

/** @brief Same room, playing the boss script shipped with the game. */
class ShippedBossScriptTest : public ScriptedBossTest {
  protected:
    ShippedBossScriptTest() : ScriptedBossTest(std::filesystem::path(RTYPE_SCRIPTS_DIR) / "Boss.wren") {}
};

TEST_F(ShippedBossScriptTest, BossScriptAttacksAndDropsAPowerUpWhenKilled) {
    ASSERT_TRUE(bound);
    run(1.0F);
    const auto bosses = entitiesOf(game::KIND_BOSS_CORE);
    ASSERT_EQ(bosses.size(), 1U);
    ASSERT_TRUE(engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().hasScriptInstance(bosses.front()));
    const float startY = engine.registry().getComponents<ecs::Transform>().get(bosses.front()).y;

    run(4.0F);
    EXPECT_NE(engine.registry().getComponents<ecs::Transform>().get(bosses.front()).y, startY) << "the boss swings";
    EXPECT_FALSE(entitiesOf(rtype::protocol::makeEntityKind("missile")).empty()) << "the boss fires";

    engine.registry().killEntity(bosses.front());
    run(0.1F);
    EXPECT_EQ(entitiesOf(rtype::protocol::makeEntityKind("power_up")).size(), 1U);
    run(10.0F);
    EXPECT_TRUE(entitiesOf(game::KIND_BOSS_CORE).empty());
}

TEST(LevelDirectorPluginTest, EachEventTypeHasItsPrefabs) {
    EXPECT_TRUE(LevelDirectorPlugin::levelPrefab("spawn", "alien_scout").has_value());
    EXPECT_TRUE(LevelDirectorPlugin::levelPrefab("boss", "boss_core").has_value());
    EXPECT_FALSE(LevelDirectorPlugin::levelPrefab("spawn", "boss_core").has_value());
    EXPECT_FALSE(LevelDirectorPlugin::levelPrefab("boss", "alien_scout").has_value());
    EXPECT_FALSE(LevelDirectorPlugin::levelPrefab("spawn", "dragon").has_value());
    EXPECT_FALSE(LevelDirectorPlugin::levelPrefab("spawn", "").has_value());
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
    EXPECT_EQ(combat().score(1), POINTS_PER_KILL);
    EXPECT_EQ(combat().score(0), 0U);
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

    EXPECT_EQ(combat().score(0), 2 * POINTS_PER_KILL);
    EXPECT_EQ(combat().score(1), POINTS_PER_KILL);
    EXPECT_EQ(combat().score(5), 0U);
}

TEST_F(GameplayTest, ShipWithoutNetStateCannotShoot) {
    const ecs::Entity entity = registry().spawnEntity();
    registry().addComponent(entity, ecs::Transform{.x = 200.0F, .y = LANE_Y});
    shoot(entity);
    run(FIXED_DT * 2);
    EXPECT_TRUE(entitiesOf(game::KIND_PLAYER_BULLET).empty());
}

namespace {

/** @brief Engine running some gameplay plugins alone, driven by the events a test publishes. */
class GameplayPluginAloneTest : public ::testing::Test {
  protected:
    GameplayPluginAloneTest() : engine([this] { return now; }) { engine.setFixedDeltaTime(FIXED_DT); }

    /** @brief Runs the engine for some seconds of game time. */
    void run(const float seconds) {
        const auto steps = static_cast<int>(seconds / FIXED_DT);
        for (int i = 0; i < steps; ++i) {
            now += static_cast<double>(FIXED_DT);
            engine.advance();
        }
    }

    double now = 0.0;
    engine::Engine engine;
};

}  // namespace

TEST_F(GameplayPluginAloneTest, BossFightPluginReportsTheStartAndTheEndOfAFight) {
    engine.addPlugin<systems::PhysicsPlugin>();
    engine.addPlugin<rtype::server::gameplay::BossFightPlugin>();
    engine.init();
    int started = 0;
    int ended = 0;
    engine.eventBus().subscribe<rtype::server::gameplay::BossFightStarted>(
        [&started](const rtype::server::gameplay::BossFightStarted& /*event*/) { ++started; });
    engine.eventBus().subscribe<rtype::server::gameplay::BossFightEnded>(
        [&ended](const rtype::server::gameplay::BossFightEnded& /*event*/) { ++ended; });

    engine.eventBus().publish(
        rtype::server::gameplay::LevelEventPlayed{.event = {.position = 0.0F,
                                                            .type = std::string(level::EVENT_BOSS),
                                                            .params = {{"prefab", std::string("boss_core")}}}});
    run(0.5F);
    EXPECT_EQ(started, 1);
    EXPECT_EQ(ended, 0);

    engine.registry().runSystem<prefabs::BossCore>(
        [this](const ecs::Entity boss, const prefabs::BossCore& /*core*/) { engine.registry().killEntity(boss); });
    run(FIXED_DT * 2);
    EXPECT_EQ(ended, 1);
}

TEST_F(GameplayPluginAloneTest, LevelDirectorHoldsTheScrollUntilEveryHoldIsOver) {
    engine.addPlugin<timeline::TimelinePlugin>();
    engine.addPlugin<LevelDirectorPlugin>(testLevel());
    engine.init();
    timeline::TimelinePlugin& player = engine.getPlugin<timeline::TimelinePlugin>();
    ecs::EventBus& bus = engine.eventBus();

    bus.publish(rtype::server::gameplay::BossFightStarted{});
    bus.publish(rtype::server::gameplay::ShipRespawnStarted{.ship = 0});
    EXPECT_FLOAT_EQ(player.speed(), 0.0F);

    bus.publish(rtype::server::gameplay::BossFightEnded{});
    EXPECT_FLOAT_EQ(player.speed(), 0.0F) << "a respawn still holds the scroll";

    bus.publish(rtype::server::gameplay::AllRespawnsOver{});
    EXPECT_FLOAT_EQ(player.speed(), LEVEL_SPEED);
}

TEST_F(GameplayPluginAloneTest, RespawnPluginBringsADestroyedShipBack) {
    engine.addPlugin<rtype::server::gameplay::RespawnPlugin>();
    engine.init();
    int over = 0;
    engine.eventBus().subscribe<rtype::server::gameplay::AllRespawnsOver>(
        [&over](const rtype::server::gameplay::AllRespawnsOver& /*event*/) { ++over; });
    ecs::Registry& registry = engine.registry();
    const ecs::Entity ship = registry.spawnEntity();
    registry.addComponent(ship, ecs::Transform{.x = 900.0F, .y = 500.0F});
    registry.addComponent(ship, game::EntityNetState{.subtype = 2});

    engine.eventBus().publish(rtype::server::gameplay::ShipDestroyed{.ship = ship});
    EXPECT_FLOAT_EQ(registry.getComponents<ecs::Transform>().get(ship).y, shipSpawnPoint(2).y);
    EXPECT_NE(registry.getComponents<game::EntityNetState>().get(ship).flags & game::FLAG_INVULNERABLE, 0);

    run(RESPAWN_DURATION + 0.1F);
    EXPECT_EQ(over, 1);
    EXPECT_EQ(registry.getComponents<game::EntityNetState>().get(ship).flags & game::FLAG_INVULNERABLE, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
