#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
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
#include "gameplay/GameplayPlugin.hpp"
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
#include "protocol/Messages.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/script/ScriptSystem.hpp"
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

/** @brief Health of the test boss: a few shots. */
constexpr std::int64_t BOSS_HEALTH = 3;
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
/** @brief Length of the test terrain: 4 columns, the last one past the section end. */
constexpr float TERRAIN_LENGTH = 200.0F;

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

TEST_F(BossFightTest, BossEventStopsTheScrollAndSpawnsTheBoss) {
    run(0.9F);
    EXPECT_TRUE(bosses().empty());
    run(0.2F);

    EXPECT_FLOAT_EQ(player().speed(), 0.0F);
    const auto spawned = bosses();
    ASSERT_EQ(spawned.size(), 1U);
    EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(spawned.front()).health, BOSS_HEALTH);
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
            EXPECT_EQ(gameplay().score(0), 0U) << "only the kill scores";
        }
    }

    EXPECT_FALSE(registry().isAlive(boss));
    EXPECT_LT(lowestHealth, BOSS_HEALTH);
    EXPECT_EQ(gameplay().score(0), POINTS_PER_KILL);
    run(FIXED_DT * 2);
    EXPECT_FLOAT_EQ(player().speed(), LEVEL_SPEED);
}

TEST_F(TerrainTest, TerrainEntersFromTheRightColumnByColumn) {
    run(1.1F);
    const auto entering = columns();
    ASSERT_FALSE(entering.empty());
    EXPECT_LT(entering.size(), 4U) << "columns are built as they enter the screen";
    for (const ecs::Entity column : entering) {
        const auto& transform = registry().getComponents<ecs::Transform>().get(column);
        EXPECT_GT(transform.x, 1280.0F);
        EXPECT_FLOAT_EQ(transform.y, 720.0F - (TERRAIN_HEIGHT / 2.0F));
        EXPECT_EQ(registry().getComponents<game::EntityNetState>().get(column).subtype,
                  TERRAIN_HEIGHT / game::TERRAIN_HEIGHT_UNIT);
    }

    run(5.0F);
    EXPECT_EQ(columns().size(), 4U);
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

TEST_F(TerrainTest, ShipTouchingTheTerrainIsDestroyed) {
    const ecs::Entity grounded = spawnShip(0, 1200.0F, 700.0F);
    const ecs::Entity flying = spawnShip(1, 1200.0F, LANE_Y);
    run(4.0F);

    EXPECT_FALSE(registry().isAlive(grounded));
    EXPECT_TRUE(registry().isAlive(flying));
}

TEST_F(TerrainTest, TerrainLeavingTheScreenIsDestroyed) {
    // The last column ends 256 px after the right edge: past the left one after (1280 + 256 + 64) / 60 = 27 s.
    run(10.0F);
    EXPECT_EQ(columns().size(), 4U);
    run(20.0F);
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
    ScriptedBossTest()
        : scriptPath(std::filesystem::temp_directory_path() /
                     ("rtype_test_boss_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed())) /
                     "TestBoss.wren"),
          engine([this] { return now; }) {
        std::filesystem::create_directories(scriptPath.parent_path());
        std::ofstream(scriptPath) << TEST_BOSS_SCRIPT;

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
        engine.addPlugin<GameplayPlugin>(std::move(built));
        engine.addPlugin<prefabs::PrefabPlugin>();
        engine.addPlugin<scripting::ScriptPlugin>();
        engine.init();
        prefabs::PrefabManager& manager = engine.getPlugin<prefabs::PrefabPlugin>().manager();
        rtype::server::gameplay::registerScriptPrefabs(manager);
        systems::ScriptSystem& scripts = engine.getPlugin<scripting::ScriptPlugin>().scriptSystem();
        scripts.setPrefabManager(&manager);
        bound = rtype::server::gameplay::bindScriptComponents(scripts);
    }

    ~ScriptedBossTest() override {
        std::error_code ignored;
        std::filesystem::remove_all(scriptPath.parent_path(), ignored);
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

TEST_F(ScriptedBossTest, ScriptOfADeadBossDoesNotCrashTheRoom) {
    run(0.2F);
    const auto bosses = entitiesOf(game::KIND_BOSS_CORE);
    ASSERT_EQ(bosses.size(), 1U);
    engine.registry().killEntity(bosses.front());
    run(1.0F);
    EXPECT_TRUE(entitiesOf(game::KIND_BOSS_CORE).empty());
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
