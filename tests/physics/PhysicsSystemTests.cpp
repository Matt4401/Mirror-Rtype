#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/PhysicsEvents.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace {

/** @brief Heap allocations made by this test binary since start (see the replaced operator new below). */
std::atomic<std::size_t> g_allocations{0};

}  // namespace

void* operator new(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* ptr = std::malloc(size == 0 ? 1 : size)) {  // NOLINT(cppcoreguidelines-no-malloc)
        return ptr;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void* ptr) noexcept { std::free(ptr); }  // NOLINT(cppcoreguidelines-no-malloc)

void operator delete[](void* ptr) noexcept { std::free(ptr); }  // NOLINT(cppcoreguidelines-no-malloc)

void operator delete(void* ptr, std::size_t /*size*/) noexcept { std::free(ptr); }  // NOLINT

void operator delete[](void* ptr, std::size_t /*size*/) noexcept { std::free(ptr); }  // NOLINT

namespace {

constexpr float FIXED_DT = 1.0F / 60.0F;

TEST(PhysicsSystem, DynamicBodyAdvancesTransform) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
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
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);

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
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);

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
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);

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
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
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
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{});
    registry.addComponent<ecs::RigidBody2D>(entity, ecs::RigidBody2D{});

    physicsSystem.update(FIXED_DT);
    ASSERT_EQ(physicsSystem.bodyCount(), 1U);

    registry.removeComponent<ecs::RigidBody2D>(entity);

    EXPECT_EQ(physicsSystem.bodyCount(), 0U);
}

constexpr std::uint16_t SHOT = 0x0001;
constexpr std::uint16_t TARGET = 0x0002;
constexpr std::uint16_t OTHER = 0x0004;

/** @brief Records every contact event published on a bus. */
struct ContactLog {
    std::vector<events::CollisionBeginEvent> begins;
    std::vector<events::CollisionEndEvent> ends;

    explicit ContactLog(ecs::EventBus& bus) {
        bus.subscribe<events::CollisionBeginEvent>(
            [this](const events::CollisionBeginEvent& event) { begins.push_back(event); });
        bus.subscribe<events::CollisionEndEvent>(
            [this](const events::CollisionEndEvent& event) { ends.push_back(event); });
    }
};

ecs::Entity spawnBody(ecs::Registry& registry, float x, ecs::RigidBody2D body, ecs::Collider2D collider) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(entity, ecs::Transform{.x = x});
    registry.addComponent<ecs::RigidBody2D>(entity, body);
    registry.addComponent<ecs::Collider2D>(entity, collider);
    return entity;
}

ecs::Entity spawnTarget(ecs::Registry& registry, float x, ecs::BodyType type = ecs::BodyType::Kinematic,
                        std::uint16_t mask = SHOT) {
    return spawnBody(
        registry, x, ecs::RigidBody2D{.bodyType = type},
        ecs::Collider2D{
            .halfWidth = 10.0F, .halfHeight = 10.0F, .categoryBits = TARGET, .maskBits = mask, .isSensor = true});
}

ecs::Entity spawnShot(ecs::Registry& registry, float x, float speed, std::uint16_t mask = TARGET) {
    return spawnBody(
        registry, x, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = speed, .bullet = true},
        ecs::Collider2D{
            .halfWidth = 2.0F, .halfHeight = 2.0F, .categoryBits = SHOT, .maskBits = mask, .isSensor = true});
}

void stepFor(systems::PhysicsSystem& physicsSystem, int steps) {
    for (int step = 0; step < steps; ++step) {
        physicsSystem.update(FIXED_DT);
    }
}

TEST(PhysicsSystemContacts, SensorShotCrossingTargetPublishesBeginThenEnd) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const ecs::Entity target = spawnTarget(registry, 100.0F);
    const ecs::Entity shot = spawnShot(registry, 0.0F, 600.0F);

    stepFor(physicsSystem, 30);  // 300 px: enters at ~88 px, leaves at ~112 px

    ASSERT_EQ(log.begins.size(), 1U);
    ASSERT_EQ(log.ends.size(), 1U);
    const events::CollisionBeginEvent& begin = log.begins.front();
    EXPECT_TRUE(begin.isSensor);
    const bool shotFirst = begin.entityA == shot;
    EXPECT_EQ(shotFirst ? begin.entityB : begin.entityA, target);
    EXPECT_EQ(shotFirst ? begin.categoryA : begin.categoryB, SHOT);
    EXPECT_EQ(shotFirst ? begin.categoryB : begin.categoryA, TARGET);
}

TEST(PhysicsSystemContacts, MaskFilteringSuppressesEvents) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnTarget(registry, 100.0F, ecs::BodyType::Kinematic, OTHER);
    spawnShot(registry, 0.0F, 600.0F);

    stepFor(physicsSystem, 30);

    EXPECT_TRUE(log.begins.empty());
    EXPECT_TRUE(log.ends.empty());
}

TEST(PhysicsSystemContacts, TwoNonDynamicBodiesNeverCollide) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnTarget(registry, 0.0F, ecs::BodyType::Kinematic, TARGET);
    spawnTarget(registry, 5.0F, ecs::BodyType::Static, TARGET);

    stepFor(physicsSystem, 5);

    EXPECT_TRUE(log.begins.empty());
}

TEST(PhysicsSystemContacts, SolidContactIsNotASensor) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnBody(registry, 100.0F, ecs::RigidBody2D{}, ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 100.0F});
    spawnBody(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 600.0F},
              ecs::Collider2D{.shape = ecs::ColliderShape::Circle, .radius = 8.0F});

    stepFor(physicsSystem, 60);

    ASSERT_FALSE(log.begins.empty());
    EXPECT_FALSE(log.begins.front().isSensor);
}

TEST(PhysicsSystemContacts, KillingAnEntityInAHandlerDropsItsOtherContacts) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);

    // A wide target overlapped by two shots in the same step: the first hit kills the target.
    const ecs::Entity target = spawnBody(
        registry, 100.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic},
        ecs::Collider2D{
            .halfWidth = 10.0F, .halfHeight = 50.0F, .categoryBits = TARGET, .maskBits = SHOT, .isSensor = true});
    const ecs::Entity first = spawnShot(registry, 0.0F, 600.0F);
    const ecs::Entity second = spawnShot(registry, 0.0F, 600.0F);
    registry.getComponents<ecs::Transform>().get(second).y = 20.0F;

    std::vector<events::CollisionBeginEvent> begins;
    bus.subscribe<events::CollisionBeginEvent>([&](const events::CollisionBeginEvent& event) {
        begins.push_back(event);
        if (registry.isAlive(target)) {
            registry.killEntity(target);
        }
    });

    EXPECT_NO_THROW(stepFor(physicsSystem, 30));

    ASSERT_EQ(begins.size(), 1U);
    const ecs::Entity shot = begins.front().entityA == target ? begins.front().entityB : begins.front().entityA;
    EXPECT_TRUE(shot == first || shot == second);
    EXPECT_EQ(physicsSystem.bodyCount(), 2U);
}

TEST(PhysicsSystemContacts, DestroyingATouchingBodyPublishesNoEnd) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const ecs::Entity target = spawnTarget(registry, 100.0F);
    spawnShot(registry, 95.0F, 0.0F);

    stepFor(physicsSystem, 2);
    ASSERT_EQ(log.begins.size(), 1U);

    registry.killEntity(target);
    stepFor(physicsSystem, 2);

    EXPECT_TRUE(log.ends.empty());
}

TEST(PhysicsSystemContacts, RemovingTheColliderPublishesEnd) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const ecs::Entity target = spawnTarget(registry, 100.0F);
    spawnShot(registry, 95.0F, 0.0F);

    stepFor(physicsSystem, 2);
    ASSERT_EQ(log.begins.size(), 1U);

    registry.removeComponent<ecs::Collider2D>(target);
    stepFor(physicsSystem, 1);

    EXPECT_EQ(log.ends.size(), 1U);
}

TEST(PhysicsSystemContacts, TwoCollidingBoxesDispatchExactlyOneBegin) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    const ecs::Entity wall =
        spawnBody(registry, 100.0F, ecs::RigidBody2D{}, ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 50.0F});
    const ecs::Entity box =
        spawnBody(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 600.0F},
                  ecs::Collider2D{.halfWidth = 8.0F, .halfHeight = 8.0F});

    stepFor(physicsSystem, 60);

    ASSERT_EQ(log.begins.size(), 1U);
    const events::CollisionBeginEvent& begin = log.begins.front();
    EXPECT_TRUE((begin.entityA == box && begin.entityB == wall) || (begin.entityA == wall && begin.entityB == box));
    EXPECT_FALSE(begin.isSensor);
    EXPECT_TRUE(begin.hasContactPoint);
}

TEST(PhysicsSystemContacts, BeginCarriesPointAndNormal) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    // Wall face at x = 95; the box hits it moving along +x.
    spawnBody(registry, 100.0F, ecs::RigidBody2D{}, ecs::Collider2D{.halfWidth = 5.0F, .halfHeight = 50.0F});
    const ecs::Entity box =
        spawnBody(registry, 0.0F, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic, .velocityX = 600.0F},
                  ecs::Collider2D{.halfWidth = 8.0F, .halfHeight = 8.0F});

    stepFor(physicsSystem, 60);

    ASSERT_EQ(log.begins.size(), 1U);
    const events::CollisionBeginEvent& begin = log.begins.front();
    ASSERT_TRUE(begin.hasContactPoint);
    // The normal points from A to B: from the box to the wall (+x) when the box is A.
    const float towardsWall = begin.entityA == box ? 1.0F : -1.0F;
    EXPECT_NEAR(begin.normalX, towardsWall, 0.01F);
    EXPECT_NEAR(begin.normalY, 0.0F, 0.01F);
    EXPECT_NEAR(begin.pointX, 95.0F, 3.0F);
    EXPECT_NEAR(begin.pointY, 0.0F, 1.0F);
}

TEST(PhysicsSystemContacts, SensorBeginHasNoContactPoint) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    ContactLog log(bus);

    spawnTarget(registry, 100.0F);
    spawnShot(registry, 95.0F, 0.0F);

    stepFor(physicsSystem, 2);

    ASSERT_EQ(log.begins.size(), 1U);
    EXPECT_TRUE(log.begins.front().isSensor);
    EXPECT_FALSE(log.begins.front().hasContactPoint);
}

TEST(PhysicsSystemContacts, ContactDispatchDoesNotAllocate) {
    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physicsSystem(registry, bus);
    std::size_t begins = 0;
    std::size_t ends = 0;
    bus.subscribe<events::CollisionBeginEvent>([&](const events::CollisionBeginEvent& /*event*/) { ++begins; });
    bus.subscribe<events::CollisionEndEvent>([&](const events::CollisionEndEvent& /*event*/) { ++ends; });

    spawnTarget(registry, 100.0F);
    const ecs::Entity shot = spawnShot(registry, 0.0F, 600.0F);
    // Each cycle teleports the shot back and lets it cross the target: one begin and one end per cycle.
    const auto runCycles = [&](int cycles) {
        for (int cycle = 0; cycle < cycles; ++cycle) {
            registry.getComponents<ecs::Transform>().get(shot).x = 0.0F;
            stepFor(physicsSystem, 30);
        }
    };

    const std::size_t startAllocations = g_allocations.load();
    runCycles(5);  // warm-up: Box2D pools, broad-phase buffers and contact buffers reach their size
    ASSERT_GT(g_allocations.load(), startAllocations) << "the allocation counter is not active";
    const std::size_t beginsBefore = begins;
    const std::size_t allocationsBefore = g_allocations.load();
    runCycles(10);
    const std::size_t allocations = g_allocations.load() - allocationsBefore;

    EXPECT_EQ(begins - beginsBefore, 10U);
    EXPECT_GE(ends, 10U);
    EXPECT_EQ(allocations, 0U);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
