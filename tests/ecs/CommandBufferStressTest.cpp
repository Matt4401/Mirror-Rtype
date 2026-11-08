#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

// Structural changes made while systems run, through the CommandBuffer: the only safe way to spawn or destroy from
// inside `runSystem`, since a direct kill would swap-and-pop the arrays being iterated.

namespace {

struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

struct Marker {
    int value;
};

/** @brief Entities alive at the start of the churn test. */
constexpr std::size_t ENTITY_COUNT = 50'000;
/** @brief Number of simulated frames in the churn test. */
constexpr int FRAME_COUNT = 20;
/** @brief Chance for each entity to be destroyed during a frame, and for a new one to be spawned in its place. */
constexpr double CHURN_RATIO = 0.3;
/** @brief Fixed seed, so that a failure can be replayed. */
constexpr std::uint32_t SEED = 40;

/** @brief Number of commands queued from inside a flush, enough to make the queues reallocate several times. */
constexpr int REENTRANT_COMMANDS = 1'000;

/** @brief Spawns an entity whose position counts the system runs that visited it. */
void deferMovingEntity(ecs::CommandBuffer& buffer, std::unordered_map<ecs::Entity, int>& runs) {
    buffer.deferSpawn([&runs](ecs::Entity e, ecs::Registry& registry) {
        registry.addComponent<Position>(e, Position{.x = 0.0F, .y = 0.0F});
        registry.addComponent<Velocity>(e, Velocity{.dx = 1.0F, .dy = 0.0F});
        runs.emplace(e, 0);
    });
}

}  // namespace

TEST(CommandBufferStress, ChurnDuringSystemRunsKeepsEveryEntityConsistent) {
    ecs::Registry registry;
    ecs::CommandBuffer buffer;
    std::mt19937 rng(SEED);
    std::bernoulli_distribution churn(CHURN_RATIO);

    // Reference model: how many system runs each live entity went through, so its position must equal that count.
    std::unordered_map<ecs::Entity, int> runs;
    std::vector<ecs::Entity> destroyed;

    for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
        deferMovingEntity(buffer, runs);
    }
    buffer.flush(registry);
    ASSERT_EQ(runs.size(), ENTITY_COUNT);

    for (int frame = 0; frame < FRAME_COUNT; ++frame) {
        std::unordered_set<ecs::Entity> doomed;
        std::size_t visited = 0;

        registry.runSystem<Position, Velocity>([&](ecs::Entity e, Position& pos, Velocity const& vel) {
            ++visited;
            auto const it = runs.find(e);
            ASSERT_NE(it, runs.end()) << "a destroyed entity was visited";
            // The value followed its entity through every swap-and-pop of the previous frames.
            ASSERT_EQ(pos.x, static_cast<float>(it->second));
            pos.x += vel.dx;
            ++it->second;

            if (churn(rng)) {
                buffer.deferDestroy(e);
                buffer.deferDestroy(e);  // duplicates must not recycle the slot twice
                doomed.insert(e);
                deferMovingEntity(buffer, runs);
            }
        });
        ASSERT_EQ(visited, runs.size());

        // Nothing changed while the system ran: the commands only apply at the flush.
        for (ecs::Entity const e : doomed) {
            ASSERT_TRUE(registry.isAlive(e));
        }
        buffer.flush(registry);
        ASSERT_TRUE(buffer.empty());

        for (ecs::Entity const e : doomed) {
            ASSERT_FALSE(registry.isAlive(e));
            runs.erase(e);
            destroyed.push_back(e);
        }
        ASSERT_EQ(registry.getComponents<Position>().getDense().size(), runs.size());
        ASSERT_EQ(registry.getComponents<Velocity>().getDense().size(), runs.size());
    }

    for (auto const& [e, count] : runs) {
        ASSERT_TRUE(registry.isAlive(e));
        ASSERT_EQ(registry.getComponents<Position>().get(e).x, static_cast<float>(count));
    }
    // A destroyed handle stays dead even once its slot holds another entity.
    for (ecs::Entity const e : destroyed) {
        ASSERT_FALSE(registry.isAlive(e));
        ASSERT_FALSE(registry.hasComponent<Position>(e));
    }
}

TEST(CommandBufferStress, CommandsQueuedDuringFlushRunAtTheNextFlush) {
    ecs::Registry registry;
    ecs::CommandBuffer buffer;
    ecs::Entity parent = ecs::NULL_ENTITY;

    // The initializer queues into the buffer being flushed: its queues grow while the flush walks them.
    buffer.deferSpawn([&](ecs::Entity e, ecs::Registry& /*registry*/) {
        parent = e;
        for (int i = 0; i < REENTRANT_COMMANDS; ++i) {
            buffer.deferSpawn(
                [i](ecs::Entity child, ecs::Registry& reg) { reg.addComponent<Marker>(child, Marker{.value = i}); });
        }
        buffer.deferDestroy(e);
    });

    buffer.flush(registry);
    ASSERT_TRUE(registry.isAlive(parent));
    EXPECT_TRUE(registry.getComponents<Marker>().getDense().empty());
    EXPECT_FALSE(buffer.empty());

    buffer.flush(registry);
    EXPECT_FALSE(registry.isAlive(parent));
    EXPECT_EQ(registry.getComponents<Marker>().getDense().size(), static_cast<std::size_t>(REENTRANT_COMMANDS));
    EXPECT_TRUE(buffer.empty());
}

TEST(CommandBufferStress, DestroyListenerQueuingDuringFlushRunsAtTheNextFlush) {
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    std::vector<ecs::Entity> children;
    for (int i = 0; i < REENTRANT_COMMANDS; ++i) {
        children.push_back(registry.spawnEntity());
    }
    ecs::Entity const parent = registry.spawnEntity();

    // Like a hierarchy cascade: destroying the parent defers the destruction of its children.
    registry.addDestroyListener([&](ecs::Entity destroyed) {
        if (destroyed == parent) {
            for (ecs::Entity const child : children) {
                buffer.deferDestroy(child);
            }
        }
    });

    buffer.deferDestroy(parent);
    buffer.flush(registry);
    EXPECT_FALSE(registry.isAlive(parent));
    for (ecs::Entity const child : children) {
        ASSERT_TRUE(registry.isAlive(child));
    }

    buffer.flush(registry);
    for (ecs::Entity const child : children) {
        ASSERT_FALSE(registry.isAlive(child));
    }
    EXPECT_TRUE(buffer.empty());
}

TEST(CommandBufferStress, MassDestroyOfStaleAndDuplicateHandlesOnlyKillsLiveTargets) {
    ecs::Registry registry;
    ecs::CommandBuffer buffer;

    std::vector<ecs::Entity> entities;
    entities.reserve(ENTITY_COUNT);
    for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
        entities.push_back(registry.spawnEntity());
    }

    // Kill every other entity now, then refill the freed slots: those handles are stale, their slots taken.
    std::vector<ecs::Entity> stale;
    std::vector<ecs::Entity> targets;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        (i % 2 == 0 ? stale : targets).push_back(entities[i]);
    }
    for (ecs::Entity const e : stale) {
        registry.killEntity(e);
    }
    std::vector<ecs::Entity> newcomers;
    newcomers.reserve(stale.size());
    for (std::size_t i = 0; i < stale.size(); ++i) {
        newcomers.push_back(registry.spawnEntity());
        registry.addComponent<Marker>(newcomers.back(), Marker{.value = static_cast<int>(i)});
    }

    for (ecs::Entity const e : stale) {
        buffer.deferDestroy(e);
    }
    for (int copy = 0; copy < 3; ++copy) {
        for (ecs::Entity const e : targets) {
            buffer.deferDestroy(e);
        }
    }
    buffer.deferDestroy(ecs::NULL_ENTITY);
    buffer.deferDestroy(ecs::makeEntity(static_cast<std::uint32_t>(ENTITY_COUNT * 4), 0U));  // never spawned
    buffer.flush(registry);

    for (ecs::Entity const e : targets) {
        ASSERT_FALSE(registry.isAlive(e));
    }
    for (std::size_t i = 0; i < newcomers.size(); ++i) {
        ASSERT_TRUE(registry.isAlive(newcomers[i]));
        ASSERT_EQ(registry.getComponents<Marker>().get(newcomers[i]).value, static_cast<int>(i));
    }

    // Each target slot was freed exactly once: refilling them all, then one more, needs exactly one new slot.
    std::unordered_set<std::uint32_t> refilled;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        ASSERT_TRUE(refilled.insert(ecs::entityIndex(registry.spawnEntity())).second) << "slot handed out twice";
    }
    EXPECT_EQ(ecs::entityIndex(registry.spawnEntity()), static_cast<std::uint32_t>(ENTITY_COUNT));
}
