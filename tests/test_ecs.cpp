#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "ecs/ComponentType.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

struct Position {
    float x, y;
};

TEST(ECS, LifecycleAndIteration) {
    ecs::Registry registry;
    registry.registerComponent<Position>();

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<Position>(e1, Position{.x = 10.0F, .y = 20.0F});

    auto& positions = registry.getComponents<Position>();
    EXPECT_TRUE(positions.contains(e1));
    EXPECT_EQ(positions.get(e1).x, 10.0F);

    registry.killEntity(e1);
    EXPECT_FALSE(positions.contains(e1));
}

TEST(ECS, EntityRecycling) {
    ecs::Registry registry;

    ecs::Entity const e1 = registry.spawnEntity();
    ecs::Entity const e2 = registry.spawnEntity();

    EXPECT_EQ(e1, 0);
    EXPECT_EQ(e2, 1);

    registry.killEntity(e1);

    ecs::Entity const e3 = registry.spawnEntity();
    EXPECT_EQ(ecs::entityIndex(e3), ecs::entityIndex(e1));  // Should recycle e1's slot...
    EXPECT_NE(e3, e1);                                      // ...with a new generation
    EXPECT_EQ(ecs::entityGeneration(e3), 1U);
}

TEST(ECS, DestroyListenerRunsBeforeComponentsAreRemoved) {
    ecs::Registry registry;
    registry.registerComponent<Position>();
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<Position>(entity, Position{.x = 1.0F, .y = 2.0F});

    std::optional<ecs::Entity> notifiedEntity;
    const std::size_t listenerId = registry.addDestroyListener([&](ecs::Entity destroyed) {
        EXPECT_TRUE(registry.hasComponent<Position>(destroyed));
        notifiedEntity = destroyed;
    });

    registry.killEntity(entity);

    EXPECT_EQ(notifiedEntity, entity);
    EXPECT_FALSE(registry.hasComponent<Position>(entity));
    registry.removeDestroyListener(listenerId);
}

TEST(ECS, ThrowingDestroyListenerStillDestroysEntity) {
    ecs::Registry registry;
    registry.registerComponent<Position>();
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<Position>(entity, Position{.x = 1.0F, .y = 2.0F});

    bool secondNotified = false;
    const std::size_t throwingId =
        registry.addDestroyListener([](ecs::Entity /*destroyed*/) { throw std::runtime_error("listener failure"); });
    const std::size_t secondId = registry.addDestroyListener([&](ecs::Entity /*destroyed*/) { secondNotified = true; });

    EXPECT_THROW(registry.killEntity(entity), std::runtime_error);

    EXPECT_TRUE(secondNotified);
    EXPECT_FALSE(registry.isAlive(entity));
    EXPECT_FALSE(registry.getComponents<Position>().contains(entity));
    const ecs::Entity recycled = registry.spawnEntity();
    EXPECT_EQ(ecs::entityIndex(recycled), ecs::entityIndex(entity));
    EXPECT_NE(recycled, entity);

    // The dispatch depth was restored: a listener removed now is compacted at once and no longer notified.
    registry.removeDestroyListener(throwingId);
    EXPECT_NO_THROW(registry.killEntity(recycled));
    registry.removeDestroyListener(secondId);
}

TEST(ECS, DestroyListenerCannotDestroyEntityRecursively) {
    ecs::Registry registry;
    registry.registerComponent<Position>();
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<Position>(entity, Position{.x = 1.0F, .y = 2.0F});

    int calls = 0;
    registry.addDestroyListener([&](ecs::Entity destroyed) {
        ++calls;
        EXPECT_FALSE(registry.isAlive(destroyed));
        EXPECT_TRUE(registry.hasComponent<Position>(destroyed));
        registry.killEntity(destroyed);
    });

    registry.killEntity(entity);

    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(registry.isAlive(entity));
    EXPECT_FALSE(registry.hasComponent<Position>(entity));
    ecs::Entity const recycled = registry.spawnEntity();
    EXPECT_EQ(ecs::entityIndex(recycled), ecs::entityIndex(entity));
    EXPECT_NE(recycled, entity);
    EXPECT_NE(ecs::entityIndex(registry.spawnEntity()), ecs::entityIndex(entity));
}

TEST(ECS, ListenerRemovalDuringDispatchSkipsUnregisteredListeners) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();
    int destroyCalls = 0;
    int componentRemoveCalls = 0;

    std::size_t firstDestroyListenerId = 0;
    std::size_t secondDestroyListenerId = 0;
    firstDestroyListenerId = registry.addDestroyListener([&](ecs::Entity) {
        ++destroyCalls;
        registry.removeDestroyListener(firstDestroyListenerId);
        registry.removeDestroyListener(secondDestroyListenerId);
    });
    secondDestroyListenerId = registry.addDestroyListener([&](ecs::Entity) { ++destroyCalls; });

    registry.killEntity(entity);
    EXPECT_EQ(destroyCalls, 1);

    const ecs::Entity componentEntity = registry.spawnEntity();
    registry.addComponent<Position>(componentEntity, Position{.x = 1.0F, .y = 2.0F});

    std::size_t firstComponentListenerId = 0;
    std::size_t secondComponentListenerId = 0;
    firstComponentListenerId = registry.addComponentRemoveListener([&](ecs::Entity, ecs::ComponentTypeId) {
        ++componentRemoveCalls;
        registry.removeComponentRemoveListener(firstComponentListenerId);
        registry.removeComponentRemoveListener(secondComponentListenerId);
    });
    secondComponentListenerId =
        registry.addComponentRemoveListener([&](ecs::Entity, ecs::ComponentTypeId) { ++componentRemoveCalls; });

    registry.removeComponent<Position>(componentEntity);
    EXPECT_EQ(componentRemoveCalls, 1);
}

TEST(ECS, SparseArraySwapAndPop) {
    ecs::Registry registry;
    registry.registerComponent<Position>();

    ecs::Entity const e1 = registry.spawnEntity();
    ecs::Entity const e2 = registry.spawnEntity();
    ecs::Entity const e3 = registry.spawnEntity();

    registry.addComponent<Position>(e1, Position{.x = 1.0F, .y = 1.0F});
    registry.addComponent<Position>(e2, Position{.x = 2.0F, .y = 2.0F});
    registry.addComponent<Position>(e3, Position{.x = 3.0F, .y = 3.0F});

    auto& positions = registry.getComponents<Position>();
    EXPECT_EQ(positions.getDense().size(), 3);

    // Erasing e2 should swap e3 into e2's place in the dense array
    registry.killEntity(e2);

    EXPECT_FALSE(positions.contains(e2));
    EXPECT_TRUE(positions.contains(e1));
    EXPECT_TRUE(positions.contains(e3));
    EXPECT_EQ(positions.getDense().size(), 2);

    // Ensure e3's data is intact after being swapped
    EXPECT_EQ(positions.get(e3).x, 3.0F);
}

struct Velocity {
    float dx, dy;
};

TEST(ECS, RunSystemView) {
    ecs::Registry registry;
    registry.registerComponent<Position>();
    registry.registerComponent<Velocity>();

    ecs::Entity const e1 = registry.spawnEntity();
    ecs::Entity const e2 = registry.spawnEntity();

    registry.addComponent<Position>(e1, Position{.x = 0.0F, .y = 0.0F});
    registry.addComponent<Velocity>(e1, Velocity{.dx = 1.0F, .dy = 2.0F});

    registry.addComponent<Position>(e2, Position{.x = 10.0F, .y = 10.0F});
    // e2 has no Velocity

    registry.runSystem<Position, Velocity>([]([[maybe_unused]] ecs::Entity e, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    auto& positions = registry.getComponents<Position>();
    EXPECT_EQ(positions.get(e1).x, 1.0F);
    EXPECT_EQ(positions.get(e1).y, 2.0F);

    // e2 should be unaffected
    EXPECT_EQ(positions.get(e2).x, 10.0F);
    EXPECT_EQ(positions.get(e2).y, 10.0F);
}

TEST(ECS, RunSystemOptimizesIterationOnSmallestArray) {
    ecs::Registry registry;
    for (int i = 0; i < 100; ++i) {
        ecs::Entity const e = registry.spawnEntity();
        registry.addComponent<Position>(e, Position{.x = static_cast<float>(i), .y = 0.0F});
    }

    ecs::Entity const rareEntity = 42;
    registry.addComponent<Velocity>(rareEntity, Velocity{.dx = 100.0F, .dy = 200.0F});

    int matchCount = 0;
    registry.runSystem<Position, Velocity>([&](ecs::Entity e, Position& pos, Velocity& vel) {
        EXPECT_EQ(e, rareEntity);
        EXPECT_FLOAT_EQ(pos.x, 42.0F);
        EXPECT_FLOAT_EQ(vel.dx, 100.0F);
        ++matchCount;
    });

    EXPECT_EQ(matchCount, 1);
}

TEST(ECS, AutoRegistration) {
    ecs::Registry registry;
    // Notice: no registerComponent call!
    ecs::Entity const e = registry.spawnEntity();
    registry.addComponent<Position>(e, Position{.x = 42.0F, .y = 84.0F});

    EXPECT_TRUE(registry.hasComponent<Position>(e));
    EXPECT_EQ(registry.getComponents<Position>().get(e).x, 42.0F);
}

TEST(ECS, EmplaceAndRemoveComponent) {
    ecs::Registry registry;
    ecs::Entity const e = registry.spawnEntity();

    registry.emplaceComponent<Position>(e, Position{.x = 5.0F, .y = 15.0F});
    EXPECT_TRUE(registry.hasComponent<Position>(e));
    EXPECT_EQ(registry.getComponents<Position>().get(e).y, 15.0F);

    registry.removeComponent<Position>(e);
    EXPECT_FALSE(registry.hasComponent<Position>(e));
}

TEST(ECS, HighThroughputIteration) {
    ecs::Registry registry;
    constexpr std::size_t entityCount = 10000;

    for (std::size_t i = 0; i < entityCount; ++i) {
        ecs::Entity const e = registry.spawnEntity();
        registry.emplaceComponent<Position>(e, Position{.x = 0.0F, .y = 0.0F});
        registry.emplaceComponent<Velocity>(e, Velocity{.dx = 1.0F, .dy = 2.0F});
    }

    registry.runSystem<Position, Velocity>([](ecs::Entity /*unused*/, Position& pos, Velocity const& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    auto const& positions = registry.getComponents<Position>();
    EXPECT_EQ(positions.get(0).x, 1.0F);
    EXPECT_EQ(positions.get(0).y, 2.0F);
    EXPECT_EQ(positions.get(entityCount - 1).x, 1.0F);
    EXPECT_EQ(positions.get(entityCount - 1).y, 2.0F);
}

#include "events/EventBus.hpp"

struct PlayerDamagedEvent {
    ecs::Entity player;
    int damage;
};

TEST(Events, EventBusPubSub) {
    ecs::EventBus bus;
    int totalDamage = 0;

    auto token =
        bus.subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent& event) { totalDamage += event.damage; });

    bus.publish(PlayerDamagedEvent{.player = 0, .damage = 10});
    EXPECT_EQ(totalDamage, 10);

    bus.unsubscribe<PlayerDamagedEvent>(token);
    bus.publish(PlayerDamagedEvent{.player = 0, .damage = 50});

    // Should still be 10 because we unsubscribed
    EXPECT_EQ(totalDamage, 10);
}

TEST(Events, UnsubscribeInsideCallbackIsSafe) {
    ecs::EventBus bus;
    ecs::EventBus::EventToken token1 = 0;
    ecs::EventBus::EventToken token2 = 0;
    int calledCount1 = 0;
    int calledCount2 = 0;

    token1 = bus.subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent&) {
        ++calledCount1;
        bus.unsubscribe<PlayerDamagedEvent>(token1);
        bus.unsubscribe<PlayerDamagedEvent>(token2);
    });

    token2 = bus.subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent&) { ++calledCount2; });

    bus.publish(PlayerDamagedEvent{.player = 0, .damage = 5});
    EXPECT_EQ(calledCount1, 1);
    EXPECT_EQ(calledCount2, 0);  // Unsubscribed before it ran in the same dispatch

    // Second publish: both should be unsubscribed
    bus.publish(PlayerDamagedEvent{.player = 0, .damage = 5});
    EXPECT_EQ(calledCount1, 1);
    EXPECT_EQ(calledCount2, 0);
}

TEST(Events, SubscribeInsideCallbackDoesNotAffectCurrentDispatch) {
    ecs::EventBus bus;
    int callCountA = 0;
    int callCountB = 0;

    bus.subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent&) {
        ++callCountA;
        bus.subscribe<PlayerDamagedEvent>([&](const PlayerDamagedEvent&) { ++callCountB; });
    });

    bus.publish(PlayerDamagedEvent{.player = 0, .damage = 5});
    EXPECT_EQ(callCountA, 1);
    EXPECT_EQ(callCountB, 0);  // Newly subscribed listener shouldn't run in the current dispatch

    bus.publish(PlayerDamagedEvent{.player = 0, .damage = 5});
    EXPECT_EQ(callCountA, 2);
    EXPECT_EQ(callCountB, 1);  // Now it runs on the subsequent publish
}

#include "ecs/CommandBuffer.hpp"

TEST(ECS, EntityLivenessAndDoubleKill) {
    ecs::Registry registry;

    EXPECT_FALSE(registry.isAlive(0));
    EXPECT_FALSE(registry.isAlive(999));

    // Calling killEntity on non-existent entity should be safe no-op
    registry.killEntity(999);
    EXPECT_FALSE(registry.isAlive(999));

    ecs::Entity const e1 = registry.spawnEntity();
    EXPECT_TRUE(registry.isAlive(e1));

    registry.killEntity(e1);
    EXPECT_FALSE(registry.isAlive(e1));

    // Second killEntity must be no-op and NOT recycle e1 twice
    registry.killEntity(e1);
    EXPECT_FALSE(registry.isAlive(e1));

    ecs::Entity const e2 = registry.spawnEntity();
    EXPECT_EQ(ecs::entityIndex(e2), ecs::entityIndex(e1));
    EXPECT_TRUE(registry.isAlive(e2));
    EXPECT_FALSE(registry.isAlive(e1));

    // Next spawn must use a new slot, NOT e1's slot again
    ecs::Entity const e3 = registry.spawnEntity();
    EXPECT_NE(ecs::entityIndex(e3), ecs::entityIndex(e1));
    EXPECT_TRUE(registry.isAlive(e3));
}

TEST(ECS, CommandBufferDeduplication) {
    ecs::Registry registry;
    ecs::CommandBuffer cmd;

    ecs::Entity const e1 = registry.spawnEntity();
    EXPECT_TRUE(registry.isAlive(e1));

    // Queue multiple destroy requests for the same entity
    cmd.deferDestroy(e1);
    cmd.deferDestroy(e1);
    cmd.deferDestroy(e1);

    cmd.flush(registry);

    EXPECT_FALSE(registry.isAlive(e1));

    // Recycle e1's slot once
    ecs::Entity const e2 = registry.spawnEntity();
    EXPECT_EQ(ecs::entityIndex(e2), ecs::entityIndex(e1));

    // Ensure e1's slot was not recycled multiple times
    ecs::Entity const e3 = registry.spawnEntity();
    EXPECT_NE(ecs::entityIndex(e3), ecs::entityIndex(e1));
}

struct GameConfig {
    int width;
    int height;
    float volume;
};

TEST(ECS, EmplaceAndGetResource) {
    ecs::Registry registry;

    EXPECT_FALSE(registry.hasResource<GameConfig>());

    registry.emplaceResource<GameConfig>(GameConfig{.width = 1920, .height = 1080, .volume = 0.8F});
    EXPECT_TRUE(registry.hasResource<GameConfig>());

    auto& config = registry.getResource<GameConfig>();
    EXPECT_EQ(config.width, 1920);
    EXPECT_EQ(config.height, 1080);
    EXPECT_FLOAT_EQ(config.volume, 0.8F);

    // Modify resource in place
    config.volume = 0.5F;
    EXPECT_FLOAT_EQ(registry.getResource<GameConfig>().volume, 0.5F);
}

TEST(ECS, AddResourceMoveAndCopy) {
    ecs::Registry registry;

    GameConfig cfg{.width = 1280, .height = 720, .volume = 1.0F};
    registry.addResource(cfg);
    EXPECT_TRUE(registry.hasResource<GameConfig>());
    EXPECT_EQ(registry.getResource<GameConfig>().width, 1280);

    // Overwrite via move
    registry.addResource(GameConfig{.width = 800, .height = 600, .volume = 0.0F});
    EXPECT_EQ(registry.getResource<GameConfig>().width, 800);
    EXPECT_FLOAT_EQ(registry.getResource<GameConfig>().volume, 0.0F);
}

TEST(ECS, HasAndRemoveResource) {
    ecs::Registry registry;

    registry.emplaceResource<GameConfig>(GameConfig{.width = 640, .height = 480, .volume = 1.0F});
    EXPECT_TRUE(registry.hasResource<GameConfig>());

    registry.removeResource<GameConfig>();
    EXPECT_FALSE(registry.hasResource<GameConfig>());

    // Removing again should be safe (no-op)
    EXPECT_NO_THROW(registry.removeResource<GameConfig>());
}

TEST(ECS, ThrowOnMissingResource) {
    ecs::Registry registry;

    EXPECT_THROW((void)registry.getResource<GameConfig>(), std::runtime_error);
}

TEST(ECS, ConstRegistryResourceAccess) {
    ecs::Registry registry;
    registry.emplaceResource<GameConfig>(GameConfig{.width = 1024, .height = 768, .volume = 0.9F});

    const ecs::Registry& constRef = registry;
    EXPECT_TRUE(constRef.hasResource<GameConfig>());
    EXPECT_EQ(constRef.getResource<GameConfig>().width, 1024);
    EXPECT_FLOAT_EQ(constRef.getResource<GameConfig>().volume, 0.9F);
}

TEST(ECS, DestroyListenerCanAddListenersDuringDispatch) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();
    int added = 0;
    int addedCalls = 0;
    std::array<int, 16> capturedState{};
    capturedState.back() = 42;

    registry.addDestroyListener([&registry, &added, &addedCalls, capturedState](ecs::Entity) {
        for (; added < 64; ++added) {
            registry.addDestroyListener([&addedCalls](ecs::Entity) { ++addedCalls; });
        }
        EXPECT_EQ(capturedState.back(), 42);
    });

    registry.killEntity(entity);
    EXPECT_EQ(addedCalls, 0);

    registry.killEntity(registry.spawnEntity());
    EXPECT_EQ(addedCalls, 64);
}

TEST(ECS, ComponentRemoveListenerCanAddListenersDuringDispatch) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent<int>(entity, 1);
    int added = 0;
    int addedCalls = 0;
    std::array<int, 16> capturedState{};
    capturedState.back() = 42;

    registry.addComponentRemoveListener(
        [&registry, &added, &addedCalls, capturedState](ecs::Entity, ecs::ComponentTypeId) {
            for (; added < 64; ++added) {
                registry.addComponentRemoveListener([&addedCalls](ecs::Entity, ecs::ComponentTypeId) { ++addedCalls; });
            }
            EXPECT_EQ(capturedState.back(), 42);
        });

    registry.removeComponent<int>(entity);
    EXPECT_EQ(addedCalls, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(ECSGeneration, HandlePackingRoundTrips) {
    constexpr ecs::Entity e = ecs::makeEntity(1234U, 56U);
    static_assert(ecs::entityIndex(e) == 1234U);
    static_assert(ecs::entityGeneration(e) == 56U);
    static_assert(ecs::makeEntity(0U, ecs::MAX_ENTITY_GENERATION + 1U) == ecs::makeEntity(0U, 0U));

    // Every handle, NULL_ENTITY included, must be exact once converted to a Wren number (double).
    static_assert(ecs::NULL_ENTITY < (ecs::Entity{1} << 53U));
    EXPECT_EQ(static_cast<ecs::Entity>(static_cast<double>(ecs::NULL_ENTITY)), ecs::NULL_ENTITY);
}

TEST(ECSGeneration, StaleHandleIsDeadAfterSlotReuse) {
    ecs::Registry registry;
    ecs::Entity const oldHandle = registry.spawnEntity();
    registry.killEntity(oldHandle);
    ecs::Entity const newHandle = registry.spawnEntity();

    ASSERT_EQ(ecs::entityIndex(newHandle), ecs::entityIndex(oldHandle));
    EXPECT_FALSE(registry.isAlive(oldHandle));
    EXPECT_TRUE(registry.isAlive(newHandle));
    EXPECT_FALSE(registry.isAlive(ecs::NULL_ENTITY));
}

TEST(ECSGeneration, StaleHandleCannotReachNewOccupantComponents) {
    ecs::Registry registry;
    ecs::Entity const oldHandle = registry.spawnEntity();
    registry.addComponent<Position>(oldHandle, Position{.x = 1.0F, .y = 1.0F});
    registry.killEntity(oldHandle);

    ecs::Entity const newHandle = registry.spawnEntity();
    registry.addComponent<Position>(newHandle, Position{.x = 2.0F, .y = 2.0F});

    auto& positions = registry.getComponents<Position>();
    EXPECT_FALSE(registry.hasComponent<Position>(oldHandle));
    EXPECT_FALSE(positions.contains(oldHandle));
    EXPECT_THROW((void)positions.get(oldHandle), std::out_of_range);

    // Removing or killing through the stale handle must not touch the new entity.
    registry.removeComponent<Position>(oldHandle);
    positions.erase(oldHandle);
    registry.killEntity(oldHandle);
    EXPECT_TRUE(registry.isAlive(newHandle));
    ASSERT_TRUE(registry.hasComponent<Position>(newHandle));
    EXPECT_EQ(positions.get(newHandle).x, 2.0F);

    // Writing through the stale handle is rejected instead of overwriting the new entity's data.
    EXPECT_THROW(registry.addComponent<Position>(oldHandle, Position{.x = 3.0F, .y = 3.0F}), std::invalid_argument);
    EXPECT_THROW(registry.emplaceComponent<Position>(oldHandle, 3.0F, 3.0F), std::invalid_argument);
    EXPECT_EQ(positions.get(newHandle).x, 2.0F);
}

TEST(ECSGeneration, SparseArrayRejectsSlotTakeoverByAnotherHandle) {
    ecs::SparseArray<Position> positions;
    ecs::Entity const newHandle = ecs::makeEntity(3U, 1U);
    ecs::Entity const staleHandle = ecs::makeEntity(3U, 0U);

    positions.insertAt(newHandle, Position{.x = 2.0F, .y = 0.0F});

    // Direct insertion through a stale handle must not steal the owner's component.
    EXPECT_THROW(positions.insertAt(staleHandle, Position{.x = 1.0F, .y = 0.0F}), std::invalid_argument);
    EXPECT_FALSE(positions.contains(staleHandle));
    ASSERT_TRUE(positions.contains(newHandle));
    EXPECT_EQ(positions.get(newHandle).x, 2.0F);
    ASSERT_EQ(positions.getDenseEntities().size(), 1U);
    EXPECT_EQ(positions.getDenseEntities().front(), newHandle);

    // Replacing through the owning handle still works, and the slot can change owner once erased.
    positions.insertAt(newHandle, Position{.x = 5.0F, .y = 0.0F});
    EXPECT_EQ(positions.get(newHandle).x, 5.0F);
    positions.erase(newHandle);
    positions.insertAt(staleHandle, Position{.x = 1.0F, .y = 0.0F});
    EXPECT_TRUE(positions.contains(staleHandle));
}

TEST(ECSGeneration, RegistryRejectsComponentsOnDeadHandles) {
    ecs::Registry registry;
    ecs::Entity const killed = registry.spawnEntity();
    registry.killEntity(killed);

    // Killed but slot not reused yet: the component would be orphaned and still visited by runSystem.
    EXPECT_THROW(registry.addComponent<Position>(killed, Position{.x = 1.0F, .y = 1.0F}), std::invalid_argument);
    EXPECT_THROW(registry.emplaceComponent<Position>(killed, 1.0F, 1.0F), std::invalid_argument);
    // Never spawned.
    EXPECT_THROW(registry.addComponent<Position>(ecs::makeEntity(42U, 0U), Position{}), std::invalid_argument);
    EXPECT_THROW(registry.addComponent<Position>(ecs::NULL_ENTITY, Position{}), std::invalid_argument);

    int visited = 0;
    registry.runSystem<Position>([&visited](ecs::Entity /*entity*/, Position& /*pos*/) { ++visited; });
    EXPECT_EQ(visited, 0);
}

TEST(ECSGeneration, SwapAndPopKeepsGenerationalHandles) {
    ecs::Registry registry;
    ecs::Entity const recycledSource = registry.spawnEntity();
    registry.killEntity(recycledSource);
    ecs::Entity const a = registry.spawnEntity();  // slot 0, generation 1
    ecs::Entity const b = registry.spawnEntity();  // slot 1, generation 0
    registry.addComponent<Position>(a, Position{.x = 1.0F, .y = 0.0F});
    registry.addComponent<Position>(b, Position{.x = 2.0F, .y = 0.0F});

    registry.removeComponent<Position>(a);

    EXPECT_FALSE(registry.hasComponent<Position>(a));
    ASSERT_TRUE(registry.hasComponent<Position>(b));
    EXPECT_EQ(registry.getComponents<Position>().get(b).x, 2.0F);

    // Now move the nonzero-generation entity: removing b swaps a into b's dense position.
    registry.addComponent<Position>(a, Position{.x = 3.0F, .y = 0.0F});
    registry.removeComponent<Position>(b);

    EXPECT_FALSE(registry.hasComponent<Position>(b));
    ASSERT_TRUE(registry.hasComponent<Position>(a));
    EXPECT_EQ(registry.getComponents<Position>().get(a).x, 3.0F);
}

TEST(ECSGeneration, DestroyListenerReceivesFullHandle) {
    ecs::Registry registry;
    registry.killEntity(registry.spawnEntity());
    ecs::Entity const entity = registry.spawnEntity();
    ASSERT_EQ(ecs::entityGeneration(entity), 1U);

    std::optional<ecs::Entity> notified;
    registry.addDestroyListener([&](ecs::Entity destroyed) { notified = destroyed; });
    registry.killEntity(entity);

    EXPECT_EQ(notified, entity);
}

TEST(ECSGeneration, GenerationWrapsAround) {
    ecs::Registry registry;
    ecs::Entity e = registry.spawnEntity();
    std::uint32_t const index = ecs::entityIndex(e);

    for (std::uint32_t i = 0; i < ecs::MAX_ENTITY_GENERATION; ++i) {
        registry.killEntity(e);
        e = registry.spawnEntity();
    }
    EXPECT_EQ(ecs::entityIndex(e), index);
    EXPECT_EQ(ecs::entityGeneration(e), ecs::MAX_ENTITY_GENERATION);

    registry.killEntity(e);
    e = registry.spawnEntity();
    EXPECT_EQ(ecs::entityIndex(e), index);
    EXPECT_EQ(ecs::entityGeneration(e), 0U);
    EXPECT_TRUE(registry.isAlive(e));
}

TEST(ECSGeneration, HandleWithGarbageHighBitsIsNotAlive) {
    ecs::Registry registry;
    ecs::Entity const e = registry.spawnEntity();
    ecs::Entity const forged = e | (ecs::Entity{1} << 60U);

    EXPECT_FALSE(registry.isAlive(forged));
    registry.killEntity(forged);
    EXPECT_TRUE(registry.isAlive(e));
}
