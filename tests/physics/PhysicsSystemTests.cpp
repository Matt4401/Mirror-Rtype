#include <gtest/gtest.h>

#include <vector>

#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "systems/physics/PhysicsEvents.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace {

constexpr float FIXED_DT = 1.0F / 60.0F;
constexpr float SENSOR_HALF_SIZE = 10.0F;

/** @brief Spawns an entity with a box sensor. */
ecs::Entity spawnSensor(ecs::Registry& registry, const float x, const ecs::RigidBody2D& rigidBody) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{.x = x});
    registry.addComponent<ecs::RigidBody2D>(entity, rigidBody);
    registry.addComponent<ecs::Collider2D>(
        entity, ecs::Collider2D{.halfWidth = SENSOR_HALF_SIZE, .halfHeight = SENSOR_HALF_SIZE, .isSensor = true});
    return entity;
}

TEST(PhysicsSystem, DynamicBodyAdvancesTransform) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(entity,
                                            ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 300.0F});

    physicsSystem.update(FIXED_DT);

    EXPECT_NEAR(registry.getComponents<ecs::Transform>().get(entity).x, 5.0F, 0.001F);
    EXPECT_EQ(physicsSystem.bodyCount(), 1U);
}

TEST(PhysicsSystem, DynamicBodyStopsAtStaticWall) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);

    const ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(wall, ecs::Transform{.x = 100.0F});
    registry.addComponent<ecs::RigidBody2D>(wall, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(wall, ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});

    const ecs::Entity movingBody = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(movingBody, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(
        movingBody,
        ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 600.0F, .maxLinearSpeed = 1000.0F});
    registry.addComponent<ecs::Collider2D>(movingBody,
                                           ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 8.0F});

    for (int step = 0; step < 60; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    const ecs::Transform& transform = registry.getComponents<ecs::Transform>().get(movingBody);
    const ecs::RigidBody2D& rigidBody = registry.getComponents<ecs::RigidBody2D>().get(movingBody);
    EXPECT_LE(transform.x, 87.1F);
    EXPECT_NEAR(rigidBody.velocityX, 0.0F, 0.1F);
}

TEST(PhysicsSystem, ColliderScaleChangesCollisionBounds) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);

    const ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(wall, ecs::Transform{.x = 100.0F});
    registry.addComponent<ecs::RigidBody2D>(wall, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(wall, ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});

    const ecs::Entity movingBody = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(movingBody, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(
        movingBody,
        ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 600.0F, .maxLinearSpeed = 1000.0F});
    registry.addComponent<ecs::Collider2D>(movingBody,
                                           ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 8.0F});

    physicsSystem.update(FIXED_DT);
    registry.getComponents<ecs::Transform>().get(movingBody).scaleX = 2.0F;

    for (int step = 0; step < 60; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    EXPECT_LE(registry.getComponents<ecs::Transform>().get(movingBody).x, 79.1F);
}

TEST(PhysicsSystem, FastBulletDoesNotPassThroughThinWall) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);

    const ecs::Entity wall = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(wall, ecs::Transform{.x = 100.0F});
    registry.addComponent<ecs::RigidBody2D>(wall, ecs::RigidBody2D{});
    registry.addComponent<ecs::Collider2D>(wall, ecs::Collider2D{.halfWidth = 1.0F, .halfHeight = 50.0F});

    const ecs::Entity projectile = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(projectile, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(
        projectile,
        ecs::RigidBody2D{
            .bodyType = ecs::BodyType::Dynamic, .velocityX = 6000.0F, .maxLinearSpeed = 7000.0F, .bullet = true});
    registry.addComponent<ecs::Collider2D>(projectile,
                                           ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 2.0F});

    for (int step = 0; step < 4; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    EXPECT_LE(registry.getComponents<ecs::Transform>().get(projectile).x, 97.1F);
}

TEST(PhysicsSystem, EntityDestructionDestroysBody) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(entity, ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    registry.killEntity(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

TEST(PhysicsSystem, RemovingRigidBodyDestroysBody) {
    ecs::Registry registry;
    systems::PhysicsSystem physicsSystem(registry);
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(entity, ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    registry.removeComponent<ecs::RigidBody2D>(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

TEST(PhysicsSystem, PublishesOneCollisionWhenColliderStartsTouching) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, &bus);
    std::vector<systems::CollisionEvent> hits;
    bus.subscribe<systems::CollisionEvent>([&hits](const systems::CollisionEvent& hit) { hits.push_back(hit); });

    const ecs::Entity bullet =
        spawnSensor(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 600.0F});
    const ecs::Entity enemy = spawnSensor(registry, 100.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic});

    physicsSystem.update(FIXED_DT);
    EXPECT_TRUE(hits.empty());

    // The bullet reaches the enemy, then stays in contact for several steps: only the start of the contact counts.
    for (int step = 0; step < 10; ++step) {
        physicsSystem.update(FIXED_DT);
    }

    ASSERT_EQ(hits.size(), 1U);
    EXPECT_TRUE((hits[0].a == bullet && hits[0].b == enemy) || (hits[0].a == enemy && hits[0].b == bullet));
}

TEST(PhysicsSystem, IdleSensorStillCollidesAfterBeingMoved) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, &bus);
    std::vector<systems::CollisionEvent> hits;
    bus.subscribe<systems::CollisionEvent>([&hits](const systems::CollisionEvent& hit) { hits.push_back(hit); });

    spawnSensor(registry, 200.0F, ecs::RigidBody2D{});
    const ecs::Entity ship = spawnSensor(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic});

    // Longer than Box2D's time to sleep: the ship has no velocity, it only moves through its Transform.
    for (int step = 0; step < 60; ++step) {
        physicsSystem.update(FIXED_DT);
    }
    ASSERT_TRUE(hits.empty());

    registry.getComponents<ecs::Transform>().get(ship).x = 200.0F;
    physicsSystem.update(FIXED_DT);
    physicsSystem.update(FIXED_DT);

    EXPECT_EQ(hits.size(), 1U);
}

TEST(PhysicsSystem, CollisionHandlerCanKillEntities) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, &bus);
    bus.subscribe<systems::CollisionEvent>([&registry](const systems::CollisionEvent& hit) {
        registry.killEntity(hit.a);
        registry.killEntity(hit.b);
    });

    spawnSensor(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic});
    spawnSensor(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic});

    physicsSystem.update(FIXED_DT);
    physicsSystem.update(FIXED_DT);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

TEST(PhysicsPlugin, PublishesCollisionsOnTheEngineBus) {
    engine::Engine engine;
    engine.addPlugin<systems::PhysicsPlugin>();
    engine.init();
    int hits = 0;
    engine.eventBus().subscribe<systems::CollisionEvent>([&hits](const systems::CollisionEvent& /*hit*/) { ++hits; });

    spawnSensor(engine.registry(), 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic});
    spawnSensor(engine.registry(), 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic});

    engine.step(FIXED_DT);

    EXPECT_EQ(hits, 1);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
