#include <gtest/gtest.h>

#include <optional>

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
    EXPECT_EQ(e3, 0);  // Should recycle e1's ID
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
    EXPECT_EQ(registry.spawnEntity(), entity);
    EXPECT_NE(registry.spawnEntity(), entity);
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
    EXPECT_EQ(e2, e1);
    EXPECT_TRUE(registry.isAlive(e2));

    // Next spawn must be a new entity, NOT e1 again
    ecs::Entity const e3 = registry.spawnEntity();
    EXPECT_NE(e3, e1);
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

    // Recycle e1 once
    ecs::Entity const e2 = registry.spawnEntity();
    EXPECT_EQ(e2, e1);

    // Ensure e1 was not recycled multiple times
    ecs::Entity const e3 = registry.spawnEntity();
    EXPECT_NE(e3, e1);
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
