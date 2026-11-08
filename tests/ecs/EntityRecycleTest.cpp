#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/SparseArray.hpp"

// Slot recycling and sparse set invariants at scale. The single-slot cases (one recycle, stale handles, generation
// wrap) are in tests/test_ecs.cpp.

namespace {

/** @brief Records which handle a component was attached to, to catch a component reaching another entity. */
struct Owner {
    ecs::Entity entity;
};

/** @brief Number of spawn and kill operations in the recycling tests. */
constexpr std::size_t OPERATION_COUNT = 50'000;
/** @brief Most entities alive at once during the recycling test. */
constexpr std::size_t MAX_ALIVE = 5'000;
/** @brief Number of entities in the swap-and-pop test. */
constexpr std::size_t ERASE_ENTITY_COUNT = 1'000;
/** @brief How often the sparse array invariants are checked during the churn. */
constexpr std::size_t CHECK_PERIOD = 1'000;
/** @brief Fixed seed, so that a failure can be replayed. */
constexpr std::uint32_t SEED = 40;

/** @brief Checks that the sparse and dense vectors agree for every stored component. */
template <typename Component>
void expectConsistent(ecs::SparseArray<Component>& array) {
    auto const& entities = array.getDenseEntities();
    auto& components = array.getDense();
    ASSERT_EQ(entities.size(), components.size());
    for (std::size_t i = 0; i < entities.size(); ++i) {
        ASSERT_TRUE(array.contains(entities[i]));
        // The sparse vector leads to this exact dense slot, not to a copy or a neighbour.
        ASSERT_EQ(&array.get(entities[i]), &components[i]);
    }
}

}  // namespace

TEST(EntityRecycle, MassRecyclingNeverRevivesAStaleHandle) {
    ecs::Registry registry;
    std::mt19937 rng(SEED);

    std::vector<ecs::Entity> alive;
    std::unordered_set<ecs::Entity> issued;
    std::unordered_map<std::uint32_t, ecs::Entity> occupantOf;  // slot index -> live handle
    std::vector<ecs::Entity> killed;
    std::uint32_t highestIndex = 0;
    std::size_t peakAlive = 0;

    for (std::size_t op = 0; op < OPERATION_COUNT; ++op) {
        bool const spawn = alive.empty() || (alive.size() < MAX_ALIVE && rng() % 2 == 0);
        if (spawn) {
            ecs::Entity const e = registry.spawnEntity();
            ASSERT_TRUE(issued.insert(e).second) << "handle " << e << " handed out twice";
            ASSERT_TRUE(occupantOf.emplace(ecs::entityIndex(e), e).second) << "slot owned by two live entities";
            registry.addComponent<Owner>(e, Owner{.entity = e});
            alive.push_back(e);
            highestIndex = std::max(highestIndex, ecs::entityIndex(e));
            peakAlive = std::max(peakAlive, alive.size());
        } else {
            std::size_t const pick = rng() % alive.size();
            ecs::Entity const e = alive[pick];
            alive[pick] = alive.back();
            alive.pop_back();
            registry.killEntity(e);
            occupantOf.erase(ecs::entityIndex(e));
            killed.push_back(e);
        }
    }

    // Freed slots are reused instead of leaked: the registry never used more slots than entities alive at once.
    EXPECT_LE(static_cast<std::size_t>(highestIndex) + 1, peakAlive);

    auto& owners = registry.getComponents<Owner>();
    EXPECT_EQ(owners.getDense().size(), alive.size());
    for (ecs::Entity const e : alive) {
        ASSERT_TRUE(registry.isAlive(e));
        ASSERT_EQ(owners.get(e).entity, e);
    }
    for (ecs::Entity const e : killed) {
        ASSERT_FALSE(registry.isAlive(e));
        ASSERT_FALSE(registry.hasComponent<Owner>(e));
        ASSERT_THROW((void)owners.get(e), std::out_of_range);
    }
}

TEST(EntityRecycle, SparseArrayStaysConsistentUnderRandomInsertAndErase) {
    ecs::SparseArray<int> array;
    std::unordered_map<ecs::Entity, int> model;
    std::mt19937 rng(SEED);
    std::uniform_int_distribution<std::uint32_t> slot(0, MAX_ALIVE - 1);

    for (std::size_t op = 0; op < OPERATION_COUNT; ++op) {
        ecs::Entity const e = ecs::makeEntity(slot(rng), 0U);
        if (rng() % 3 == 0) {
            array.erase(e);
            model.erase(e);
        } else {
            int const value = static_cast<int>(op);
            array.insertAt(e, value);
            model[e] = value;
        }
        if (op % CHECK_PERIOD == 0) {
            ASSERT_NO_FATAL_FAILURE(expectConsistent(array));
        }
    }

    ASSERT_NO_FATAL_FAILURE(expectConsistent(array));
    ASSERT_EQ(array.getDense().size(), model.size());
    for (auto const& [e, value] : model) {
        ASSERT_EQ(array.get(e), value);
    }
}

TEST(EntityRecycle, EmptyOrSparseSlotsAreReportedMissing) {
    ecs::SparseArray<int> array;
    ecs::SparseArray<int> const& constArray = array;
    ecs::Entity const farAway = ecs::makeEntity(ecs::MAX_ENTITY_INDEX - 1, 0U);

    // Never filled: every lookup misses, nothing is allocated for a far index.
    EXPECT_FALSE(array.contains(0));
    EXPECT_FALSE(array.contains(farAway));
    EXPECT_FALSE(array.contains(ecs::NULL_ENTITY));
    EXPECT_THROW((void)array.get(0), std::out_of_range);
    EXPECT_THROW((void)constArray.get(farAway), std::out_of_range);
    EXPECT_NO_THROW(array.erase(farAway));
    EXPECT_NO_THROW(array.erase(ecs::NULL_ENTITY));
    EXPECT_TRUE(array.getDense().empty());

    // Filled at index 10 only: the indices before it are holes, the ones after it are out of the sparse vector.
    array.insertAt(ecs::makeEntity(10U, 0U), 7);
    EXPECT_FALSE(array.contains(ecs::makeEntity(5U, 0U)));
    EXPECT_THROW((void)array.get(ecs::makeEntity(5U, 0U)), std::out_of_range);
    EXPECT_FALSE(array.contains(ecs::makeEntity(11U, 0U)));
    EXPECT_NO_THROW(array.erase(ecs::makeEntity(5U, 0U)));
    EXPECT_EQ(array.get(ecs::makeEntity(10U, 0U)), 7);
}

TEST(EntityRecycle, SwapAndPopMovesOnlyTheLastComponent) {
    ecs::SparseArray<int> array;
    for (std::uint32_t i = 0; i < ERASE_ENTITY_COUNT; ++i) {
        array.insertAt(ecs::makeEntity(i, 0U), static_cast<int>(i));
    }
    std::mt19937 rng(SEED);

    while (array.getDense().size() > 1) {
        auto const& entities = array.getDenseEntities();
        std::unordered_map<ecs::Entity, std::size_t> before;
        for (std::size_t i = 0; i < entities.size(); ++i) {
            before.emplace(entities[i], i);
        }
        std::size_t const erasedPos = rng() % entities.size();
        ecs::Entity const erased = entities[erasedPos];
        ecs::Entity const last = entities.back();

        array.erase(erased);

        // O(1): one element moved (the last one, into the hole), every other one kept its place.
        ASSERT_EQ(array.getDense().size(), before.size() - 1);
        for (std::size_t i = 0; i < entities.size(); ++i) {
            ecs::Entity const e = entities[i];
            ASSERT_EQ(i, e == last ? erasedPos : before.at(e));
            ASSERT_EQ(array.getDense()[i], static_cast<int>(ecs::entityIndex(e)));
        }
    }
}
