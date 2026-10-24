#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "prefabs/PrefabRegistry.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "prefabs/power_up/PowerUp.hpp"

namespace {

TEST(PrefabManager, UnknownPrefabQueuesNothing) {
    prefabs::PrefabManager manager;
    ecs::CommandBuffer buffer;

    EXPECT_FALSE(manager.has("does_not_exist"));
    EXPECT_FALSE(manager.spawn("does_not_exist", buffer));
    EXPECT_TRUE(buffer.empty());
}

TEST(PrefabManager, SpawnIsDeferredUntilFlush) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::MISSILE_PREFAB, prefabs::buildMissile);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(prefabs::MISSILE_PREFAB, buffer));
    EXPECT_FALSE(buffer.empty());
    EXPECT_FALSE(registry.isAlive(0));

    buffer.flush(registry);

    EXPECT_TRUE(buffer.empty());
    EXPECT_TRUE(registry.isAlive(0));
}

TEST(PrefabManager, MissileHasAllComponents) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::MISSILE_PREFAB, prefabs::buildMissile);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::MISSILE_PREFAB, buffer, {{"x", 800.0F}, {"y", 300.0F}});
    buffer.flush(registry);
    ecs::Entity const missile = 0;

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(missile));
    ASSERT_TRUE(registry.hasComponent<ecs::Renderable>(missile));
    ASSERT_TRUE(registry.hasComponent<ecs::RigidBody2D>(missile));
    ASSERT_TRUE(registry.hasComponent<ecs::Collider2D>(missile));

    auto const& transform = registry.getComponents<ecs::Transform>().get(missile);
    EXPECT_FLOAT_EQ(transform.x, 800.0F);
    EXPECT_FLOAT_EQ(transform.y, 300.0F);
    auto const& body = registry.getComponents<ecs::RigidBody2D>().get(missile);
    EXPECT_EQ(body.bodyType, ecs::BodyType::Dynamic);
    EXPECT_FLOAT_EQ(body.velocityX, prefabs::DEFAULT_MISSILE_SPEED);
    EXPECT_TRUE(body.bullet);
}

TEST(PrefabManager, MissileVariantUsesCustomSpeed) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::MISSILE_PREFAB, prefabs::buildMissile);
    manager.registerPrefab("fast_missile", prefabs::buildMissile, {{"speed", -1200.0F}});
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::MISSILE_PREFAB, buffer);
    manager.spawn("fast_missile", buffer);
    buffer.flush(registry);

    auto const& bodies = registry.getComponents<ecs::RigidBody2D>();
    EXPECT_FLOAT_EQ(bodies.get(0).velocityX, prefabs::DEFAULT_MISSILE_SPEED);
    EXPECT_FLOAT_EQ(bodies.get(1).velocityX, -1200.0F);
}

TEST(PrefabManager, SalvoOfFiveMissilesDuringIteration) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::MISSILE_PREFAB, prefabs::buildMissile);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ecs::Entity const boss = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(boss, ecs::Transform{.x = 900.0F, .y = 400.0F});

    // Spawning while iterating a sparse array is safe: nothing is created until flush.
    for (auto const& transform : registry.getComponents<ecs::Transform>().getDense()) {
        for (int i = 0; i < 5; ++i) {
            manager.spawn(prefabs::MISSILE_PREFAB, buffer,
                          {{"x", transform.x}, {"y", transform.y + static_cast<float>(i * 20)}});
        }
    }
    EXPECT_EQ(registry.getComponents<ecs::Transform>().getDense().size(), 1U);

    buffer.flush(registry);

    EXPECT_EQ(registry.getComponents<ecs::RigidBody2D>().getDense().size(), 5U);
    EXPECT_EQ(registry.getComponents<ecs::Transform>().getDense().size(), 6U);
}

TEST(PrefabManager, PowerUpIsDriftingSensor) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::POWER_UP_PREFAB, prefabs::buildPowerUp);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(prefabs::POWER_UP_PREFAB, buffer, {{"x", 500.0F}, {"y", 200.0F}}));
    buffer.flush(registry);
    ecs::Entity const powerUp = 0;

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(powerUp));
    ASSERT_TRUE(registry.hasComponent<ecs::Renderable>(powerUp));
    ASSERT_TRUE(registry.hasComponent<ecs::RigidBody2D>(powerUp));
    ASSERT_TRUE(registry.hasComponent<ecs::Collider2D>(powerUp));

    auto const& body = registry.getComponents<ecs::RigidBody2D>().get(powerUp);
    EXPECT_EQ(body.bodyType, ecs::BodyType::Kinematic);
    EXPECT_FLOAT_EQ(body.velocityX, prefabs::DEFAULT_POWER_UP_DRIFT_SPEED);
    auto const& collider = registry.getComponents<ecs::Collider2D>().get(powerUp);
    EXPECT_EQ(collider.shape, ecs::ColliderShape::Circle);
    EXPECT_TRUE(collider.isSensor);
}

TEST(PrefabManager, ObstacleIsStaticSolidBox) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::OBSTACLE_PREFAB, prefabs::buildObstacle);
    manager.registerPrefab("obstacle_wide", prefabs::buildObstacle, {{"halfWidth", 128.0F}, {"halfHeight", 16.0F}});
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::OBSTACLE_PREFAB, buffer, {{"x", 300.0F}, {"y", 400.0F}});
    manager.spawn("obstacle_wide", buffer, {{"x", 600.0F}, {"y", 400.0F}});
    buffer.flush(registry);

    auto const& bodies = registry.getComponents<ecs::RigidBody2D>();
    EXPECT_EQ(bodies.get(0).bodyType, ecs::BodyType::Static);
    auto const& colliders = registry.getComponents<ecs::Collider2D>();
    EXPECT_FALSE(colliders.get(0).isSensor);
    EXPECT_FLOAT_EQ(colliders.get(0).halfWidth, prefabs::DEFAULT_OBSTACLE_HALF_WIDTH);
    EXPECT_FLOAT_EQ(colliders.get(1).halfWidth, 128.0F);
    EXPECT_FLOAT_EQ(colliders.get(1).halfHeight, 16.0F);
}

TEST(PrefabManager, PlayerBulletFliesTowardsEnemies) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::PLAYER_BULLET_PREFAB, prefabs::buildPlayerBullet);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(prefabs::PLAYER_BULLET_PREFAB, buffer, {{"x", 100.0F}, {"y", 300.0F}}));
    buffer.flush(registry);

    auto const& body = registry.getComponents<ecs::RigidBody2D>().get(0);
    EXPECT_EQ(body.bodyType, ecs::BodyType::Dynamic);
    EXPECT_FLOAT_EQ(body.velocityX, prefabs::DEFAULT_PLAYER_BULLET_SPEED);
    EXPECT_TRUE(body.bullet);
    EXPECT_EQ(registry.getComponents<ecs::Collider2D>().get(0).categoryBits, prefabs::collision::PLAYER_SHOT);
}

TEST(PrefabManager, AlienScoutHasNoScriptYet) {
    prefabs::PrefabManager manager;
    manager.registerPrefab(prefabs::ALIEN_SCOUT_PREFAB, prefabs::buildAlienScout);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    ASSERT_TRUE(manager.spawn(prefabs::ALIEN_SCOUT_PREFAB, buffer, {{"x", 800.0F}, {"y", 300.0F}}));
    buffer.flush(registry);

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(0));
    ASSERT_TRUE(registry.hasComponent<ecs::Renderable>(0));
    ASSERT_TRUE(registry.hasComponent<ecs::RigidBody2D>(0));
    ASSERT_TRUE(registry.hasComponent<ecs::Collider2D>(0));
    EXPECT_EQ(registry.getComponents<ecs::RigidBody2D>().get(0).bodyType, ecs::BodyType::Kinematic);
    EXPECT_FLOAT_EQ(registry.getComponents<ecs::RigidBody2D>().get(0).velocityX, prefabs::DEFAULT_ALIEN_SCOUT_SPEED);
    EXPECT_EQ(registry.getComponents<ecs::Collider2D>().get(0).categoryBits, prefabs::collision::ENEMY);
}

TEST(PrefabParams, NumbersConvertOtherTypesFallBack) {
    prefabs::PrefabParams params{{"speed", 10}, {"scale", 2.5F}, {"tag", "boss"}, {"visible", true}};

    EXPECT_FLOAT_EQ(params.get("speed", 0.0F), 10.0F);
    EXPECT_EQ(params.get("scale", 0), 2);
    EXPECT_EQ(params.get<std::string>("tag", ""), "boss");
    EXPECT_TRUE(params.get("visible", false));
    EXPECT_FLOAT_EQ(params.get("tag", 1.0F), 1.0F);
    EXPECT_FLOAT_EQ(params.get("missing", 3.0F), 3.0F);
    EXPECT_FALSE(params.find<bool>("speed").has_value());
}

TEST(PrefabParams, MergeOverridesExistingKeys) {
    prefabs::PrefabParams params{{"x", 1.0F}, {"y", 2.0F}};
    params.merge(prefabs::PrefabParams{}.set("y", 5.0F).set("rotation", 0.5F));

    EXPECT_FLOAT_EQ(params.get("x", 0.0F), 1.0F);
    EXPECT_FLOAT_EQ(params.get("y", 0.0F), 5.0F);
    EXPECT_FLOAT_EQ(params.get("rotation", 0.0F), 0.5F);
}

TEST(PrefabManager, SpawnParamsOverrideRegistrationDefaults) {
    prefabs::PrefabManager manager;
    manager.registerPrefab("fast_missile", prefabs::buildMissile, {{"speed", -1200.0F}, {"y", 50.0F}});
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn("fast_missile", buffer, {{"x", 10.0F}, {"rotation", 1.5F}, {"scaleX", -1.0F}, {"speed", -300.0F}});
    buffer.flush(registry);

    auto const& transform = registry.getComponents<ecs::Transform>().get(0);
    EXPECT_FLOAT_EQ(transform.x, 10.0F);
    EXPECT_FLOAT_EQ(transform.y, 50.0F);
    EXPECT_FLOAT_EQ(transform.rotation, 1.5F);
    EXPECT_FLOAT_EQ(transform.scaleX, -1.0F);
    EXPECT_FLOAT_EQ(registry.getComponents<ecs::RigidBody2D>().get(0).velocityX, -300.0F);
}

TEST(PrefabManager, ObstacleSizeCanBeSetPerSpawn) {
    prefabs::PrefabManager manager;
    prefabs::registerRtypePrefabs(manager);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::OBSTACLE_PREFAB, buffer, {{"halfWidth", 200.0F}, {"halfHeight", 8}});
    buffer.flush(registry);

    auto const& collider = registry.getComponents<ecs::Collider2D>().get(0);
    EXPECT_FLOAT_EQ(collider.halfWidth, 200.0F);
    EXPECT_FLOAT_EQ(collider.halfHeight, 8.0F);
}

TEST(PrefabManager, CollisionCategoriesFilterPairs) {
    prefabs::PrefabManager manager;
    prefabs::registerRtypePrefabs(manager);
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    manager.spawn(prefabs::MISSILE_PREFAB, buffer);
    manager.spawn(prefabs::PLAYER_BULLET_PREFAB, buffer);
    manager.spawn(prefabs::ALIEN_SCOUT_PREFAB, buffer);
    manager.spawn(prefabs::OBSTACLE_PREFAB, buffer);
    manager.spawn(prefabs::POWER_UP_PREFAB, buffer);
    buffer.flush(registry);

    auto const& colliders = registry.getComponents<ecs::Collider2D>();
    auto const& missile = colliders.get(0);
    auto const& playerBullet = colliders.get(1);
    auto const& alienScout = colliders.get(2);
    auto const& obstacle = colliders.get(3);
    auto const& powerUp = colliders.get(4);
    ecs::Collider2D const player{.categoryBits = prefabs::collision::PLAYER,
                                 .maskBits = prefabs::collision::PLAYER_MASK};

    using prefabs::collision::canCollide;
    EXPECT_TRUE(canCollide(playerBullet, alienScout));
    EXPECT_TRUE(canCollide(playerBullet, obstacle));
    EXPECT_FALSE(canCollide(playerBullet, player));
    EXPECT_FALSE(canCollide(playerBullet, missile));

    EXPECT_TRUE(canCollide(missile, player));
    EXPECT_TRUE(canCollide(missile, obstacle));
    EXPECT_FALSE(canCollide(missile, alienScout));

    EXPECT_TRUE(canCollide(alienScout, player));
    EXPECT_FALSE(canCollide(alienScout, powerUp));
    EXPECT_TRUE(canCollide(powerUp, player));
}

TEST(PrefabManager, SpawningSurvivesManagerDestructionBeforeFlush) {
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    {
        prefabs::PrefabManager localManager;
        localManager.registerPrefab(prefabs::MISSILE_PREFAB, prefabs::buildMissile);
        ASSERT_TRUE(localManager.spawn(prefabs::MISSILE_PREFAB, buffer, {{"x", 123.0F}}));
    }

    buffer.flush(registry);

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(0));
    EXPECT_FLOAT_EQ(registry.getComponents<ecs::Transform>().get(0).x, 123.0F);
}

}  // namespace

TEST(PrefabPlugin, InitFailsAndKeepsExistingManagerOnDuplicate) {
    engine::Engine engine;
    auto const existing = std::make_shared<prefabs::PrefabManager>();
    ASSERT_TRUE(engine.mediator().registerService<prefabs::PrefabManager>(existing).has_value());

    engine.addPlugin<prefabs::PrefabPlugin>();

    EXPECT_THROW(engine.init(), engine::MediatorError);
    EXPECT_EQ(&engine.mediator().service<prefabs::PrefabManager>(), existing.get());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
