#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "components/InputBuffer.hpp"
#include "components/NetworkId.hpp"
#include "components/NetworkOwner.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/InputQueue.hpp"
#include "net/NetworkEntityMap.hpp"

namespace {

ecs::NetInput input(const std::uint32_t sequence) { return ecs::NetInput{.sequence = sequence, .actions = 0}; }

/** @brief Pops every pending input and returns the last processed sequence. */
std::uint32_t drain(ecs::InputBuffer& buffer) {
    while (ecs::net::popInput(buffer)) {
    }
    return buffer.lastProcessed;
}

}  // namespace

TEST(NetworkComponents, DefaultValues) {
    EXPECT_EQ(ecs::NetworkId{}.value, 0U);
    EXPECT_EQ(ecs::NetworkOwner{}.client, 0U);
    EXPECT_EQ(ecs::Replicated{}.kind, 0U);
    const ecs::InputBuffer buffer;
    EXPECT_TRUE(buffer.pending.empty());
    EXPECT_EQ(buffer.lastProcessed, 0U);
    EXPECT_EQ(buffer.capacity, ecs::DEFAULT_MAX_PENDING_INPUTS);
}

TEST(NetworkComponents, WorkAsEcsComponents) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();
    registry.emplaceComponent<ecs::NetworkId>(entity, 7U);
    registry.emplaceComponent<ecs::NetworkOwner>(entity, 3U);
    registry.emplaceComponent<ecs::Replicated>(entity, static_cast<std::uint16_t>(2));
    registry.emplaceComponent<ecs::InputBuffer>(entity);

    int visited = 0;
    registry.runSystem<ecs::NetworkId, ecs::NetworkOwner, ecs::Replicated, ecs::InputBuffer>(
        [&](ecs::Entity e, const ecs::NetworkId& id, const ecs::NetworkOwner& owner, const ecs::Replicated& replicated,
            const ecs::InputBuffer&) {
            EXPECT_EQ(e, entity);
            EXPECT_EQ(id.value, 7U);
            EXPECT_EQ(owner.client, 3U);
            EXPECT_EQ(replicated.kind, 2U);
            ++visited;
        });
    EXPECT_EQ(visited, 1);
}

TEST(InputQueue, PopsInSequenceOrder) {
    ecs::InputBuffer buffer;
    EXPECT_TRUE(ecs::net::pushInput(buffer, input(3)));
    EXPECT_TRUE(ecs::net::pushInput(buffer, input(1)));
    EXPECT_TRUE(ecs::net::pushInput(buffer, input(2)));

    for (std::uint32_t expected = 1; expected <= 3; ++expected) {
        const auto popped = ecs::net::popInput(buffer);
        ASSERT_TRUE(popped.has_value());
        EXPECT_EQ(popped->sequence, expected);
        EXPECT_EQ(buffer.lastProcessed, expected);
    }
    EXPECT_FALSE(ecs::net::popInput(buffer).has_value());
}

TEST(InputQueue, RejectsDuplicatesAndAlreadyProcessed) {
    ecs::InputBuffer buffer;
    EXPECT_TRUE(ecs::net::pushInput(buffer, input(5)));
    EXPECT_FALSE(ecs::net::pushInput(buffer, input(5)));
    (void)ecs::net::popInput(buffer);

    EXPECT_FALSE(ecs::net::pushInput(buffer, input(5)));
    EXPECT_FALSE(ecs::net::pushInput(buffer, input(4)));
    EXPECT_TRUE(buffer.pending.empty());
}

TEST(InputQueue, OverflowDropsTheOldest) {
    ecs::InputBuffer buffer{.capacity = 3};
    for (std::uint32_t seq = 1; seq <= 5; ++seq) {
        ecs::net::pushInput(buffer, input(seq));
    }
    ASSERT_EQ(buffer.pending.size(), 3U);
    EXPECT_EQ(buffer.pending.front().sequence, 3U);
    EXPECT_EQ(drain(buffer), 5U);
}

TEST(InputQueue, ZeroCapacityKeepsNothing) {
    ecs::InputBuffer buffer{.capacity = 0};
    EXPECT_FALSE(ecs::net::pushInput(buffer, input(1)));
    EXPECT_TRUE(buffer.pending.empty());
}

TEST(NetworkEntityMap, AssignGivesUniqueIdsStartingAtOne) {
    ecs::net::NetworkEntityMap map;
    const auto first = map.assign(10);
    const auto second = map.assign(20);

    EXPECT_EQ(first, 1U);
    EXPECT_EQ(second, 2U);
    EXPECT_EQ(map.assign(10), first);  // already mapped: same id
    EXPECT_EQ(map.entityOf(second), std::optional<ecs::Entity>{20});
    EXPECT_EQ(map.idOf(10), std::optional<std::uint32_t>{first});
    EXPECT_EQ(map.size(), 2U);
}

TEST(NetworkEntityMap, IdsAreNotReusedAfterUnbind) {
    ecs::net::NetworkEntityMap map;
    const auto id = map.assign(10);
    map.unbindEntity(10);

    EXPECT_FALSE(map.entityOf(id).has_value());
    EXPECT_NE(map.assign(10), id);
}

TEST(NetworkEntityMap, BindReplacesPreviousMappings) {
    ecs::net::NetworkEntityMap map;
    EXPECT_FALSE(map.bind(0, 1));
    EXPECT_TRUE(map.bind(42, 1));
    EXPECT_TRUE(map.bind(42, 2));  // id moves to another entity
    EXPECT_FALSE(map.idOf(1).has_value());
    EXPECT_EQ(map.entityOf(42), std::optional<ecs::Entity>{2});

    EXPECT_TRUE(map.bind(43, 2));  // entity gets another id
    EXPECT_FALSE(map.entityOf(42).has_value());
    EXPECT_EQ(map.size(), 1U);

    map.unbind(43);
    EXPECT_EQ(map.size(), 0U);
}

TEST(NetworkEntityMap, WorksAsRegistryResource) {
    ecs::Registry registry;
    auto& map = registry.emplaceResource<ecs::net::NetworkEntityMap>();
    const ecs::Entity entity = registry.spawnEntity();
    registry.emplaceComponent<ecs::NetworkId>(entity, map.assign(entity));

    const auto& id = registry.getComponents<ecs::NetworkId>().get(entity);
    EXPECT_EQ(map.entityOf(id.value), std::optional<ecs::Entity>{entity});
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
