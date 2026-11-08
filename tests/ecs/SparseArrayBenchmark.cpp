#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/SparseArray.hpp"

// Compares the engine's sparse set with the two storages it replaces, on the same data and the same work:
// - `std::unordered_map<Entity, Component>`, one map per component type;
// - an OOP design: a vector of heap-allocated objects with a virtual `update()`.
// See docs/comparative-study/ecs-benchmark.md for the results and how to run it.

namespace {

struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

/** @brief Fixed seed, so that every run builds the same containers. */
constexpr std::uint32_t SEED = 40;
/** @brief Elements erased per iteration of the erase benchmarks, whatever the container size. */
constexpr std::size_t ERASE_COUNT = 1'000;
/** @brief Bytes of unrelated state in an OOP object (name, flags, pointers to other systems...). */
constexpr std::size_t OOP_EXTRA_STATE = 48;
/** @brief Largest unrelated allocation made between two OOP objects, to spread them on the heap like a real game. */
constexpr std::size_t MAX_NOISE_BYTES = 256;

/** @brief Base class of the OOP baseline. */
class GameObject {
  public:
    GameObject() = default;
    virtual ~GameObject() = default;
    GameObject(GameObject const&) = delete;
    GameObject& operator=(GameObject const&) = delete;
    GameObject(GameObject&&) = delete;
    GameObject& operator=(GameObject&&) = delete;

    /** @brief Advances the object by one step. */
    virtual void update() = 0;
};

/** @brief Moving object of the OOP baseline: the same data as a Position and a Velocity, plus unrelated state. */
class MovingObject final : public GameObject {
  public:
    explicit MovingObject(float x) : _position{.x = x, .y = 0.0F} {}

    void update() override {
        _position.x += _velocity.dx;
        _position.y += _velocity.dy;
    }

  private:
    Position _position;
    Velocity _velocity{.dx = 1.0F, .dy = 2.0F};
    std::array<std::byte, OOP_EXTRA_STATE> _otherState{};
};

/** @brief OOP world: the objects, and the unrelated allocations that sit between them on the heap. */
struct OopWorld {
    std::vector<std::unique_ptr<GameObject>> objects;
    std::vector<std::unique_ptr<std::byte[]>> noise;  // NOLINT(cppcoreguidelines-avoid-c-arrays)
};

/** @brief Builds an OOP world of `count` objects, scattered on the heap and visited in a shuffled order. */
OopWorld makeOopWorld(std::size_t count) {
    OopWorld world;
    std::mt19937 rng(SEED);
    std::uniform_int_distribution<std::size_t> noiseSize(1, MAX_NOISE_BYTES);
    world.objects.reserve(count);
    world.noise.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        world.objects.push_back(std::make_unique<MovingObject>(static_cast<float>(i)));
        world.noise.push_back(
            std::make_unique<std::byte[]>(noiseSize(rng)));  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    }
    std::ranges::shuffle(world.objects, rng);
    return world;
}

/** @brief Builds a registry of `count` entities that all have a Position and a Velocity. */
std::unique_ptr<ecs::Registry> makeRegistry(std::size_t count) {
    auto registry = std::make_unique<ecs::Registry>();
    for (std::size_t i = 0; i < count; ++i) {
        ecs::Entity const e = registry->spawnEntity();
        registry->addComponent<Position>(e, Position{.x = static_cast<float>(i), .y = 0.0F});
        registry->addComponent<Velocity>(e, Velocity{.dx = 1.0F, .dy = 2.0F});
    }
    return registry;
}

/** @brief Returns `count` entities with consecutive slot indices, in a shuffled order. */
std::vector<ecs::Entity> shuffledEntities(std::size_t count) {
    std::vector<ecs::Entity> entities(count);
    for (std::size_t i = 0; i < count; ++i) {
        entities[i] = ecs::makeEntity(static_cast<std::uint32_t>(i), 0U);
    }
    std::mt19937 rng(SEED);
    std::ranges::shuffle(entities, rng);
    return entities;
}

/** @brief Container size of the current run. */
std::size_t sizeOf(benchmark::State const& state) { return static_cast<std::size_t>(state.range(0)); }

// --- Iteration: move every entity by its velocity ---------------------------------------------------------------

void iterateSparseSet(benchmark::State& state) {
    auto const registry = makeRegistry(sizeOf(state));
    for (auto _ : state) {
        registry->runSystem<Position, Velocity>([](ecs::Entity /*e*/, Position& pos, Velocity const& vel) {
            pos.x += vel.dx;
            pos.y += vel.dy;
        });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

/** @brief Same work with the two values in one component: no lookup in a second array, only the dense walk. */
void iterateSparseSetOneComponent(benchmark::State& state) {
    struct Motion {
        Position position;
        Velocity velocity;
    };
    ecs::Registry registry;
    for (std::size_t i = 0; i < sizeOf(state); ++i) {
        registry.addComponent<Motion>(
            registry.spawnEntity(),
            Motion{.position = {.x = static_cast<float>(i), .y = 0.0F}, .velocity = {.dx = 1.0F, .dy = 2.0F}});
    }
    for (auto _ : state) {
        registry.runSystem<Motion>([](ecs::Entity /*e*/, Motion& motion) {
            motion.position.x += motion.velocity.dx;
            motion.position.y += motion.velocity.dy;
        });
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

void iterateUnorderedMap(benchmark::State& state) {
    std::unordered_map<ecs::Entity, Position> positions;
    std::unordered_map<ecs::Entity, Velocity> velocities;
    for (ecs::Entity const e : shuffledEntities(sizeOf(state))) {
        positions.emplace(e, Position{.x = static_cast<float>(ecs::entityIndex(e)), .y = 0.0F});
        velocities.emplace(e, Velocity{.dx = 1.0F, .dy = 2.0F});
    }
    for (auto _ : state) {
        for (auto& [e, pos] : positions) {
            if (auto const it = velocities.find(e); it != velocities.end()) {
                pos.x += it->second.dx;
                pos.y += it->second.dy;
            }
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

void iterateOop(benchmark::State& state) {
    OopWorld const world = makeOopWorld(sizeOf(state));
    for (auto _ : state) {
        for (auto const& object : world.objects) {
            object->update();
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

// --- Erase: remove ERASE_COUNT random elements from a container of the given size -------------------------------
// The container is built once. After each timed batch, the erased elements are put back untimed, so the setup cost
// does not grow with the container size.

void eraseSparseSet(benchmark::State& state) {
    std::vector<ecs::Entity> const order = shuffledEntities(sizeOf(state));
    ecs::SparseArray<Position> array;
    for (ecs::Entity const e : order) {
        array.insertAt(e, Position{.x = 0.0F, .y = 0.0F});
    }
    for (auto _ : state) {
        for (std::size_t i = 0; i < ERASE_COUNT; ++i) {
            array.erase(order[i]);
        }
        benchmark::DoNotOptimize(array.getDense().data());
        state.PauseTiming();
        for (std::size_t i = 0; i < ERASE_COUNT; ++i) {
            array.insertAt(order[i], Position{.x = 0.0F, .y = 0.0F});
        }
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * ERASE_COUNT));
}

void eraseUnorderedMap(benchmark::State& state) {
    std::vector<ecs::Entity> const order = shuffledEntities(sizeOf(state));
    std::unordered_map<ecs::Entity, Position> map;
    for (ecs::Entity const e : order) {
        map.emplace(e, Position{.x = 0.0F, .y = 0.0F});
    }
    for (auto _ : state) {
        for (std::size_t i = 0; i < ERASE_COUNT; ++i) {
            map.erase(order[i]);
        }
        benchmark::DoNotOptimize(map.size());
        state.PauseTiming();
        for (std::size_t i = 0; i < ERASE_COUNT; ++i) {
            map.emplace(order[i], Position{.x = 0.0F, .y = 0.0F});
        }
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * ERASE_COUNT));
}

void eraseOop(benchmark::State& state) {
    OopWorld world = makeOopWorld(sizeOf(state));
    std::vector<std::unique_ptr<GameObject>> erased;
    erased.reserve(ERASE_COUNT);
    std::mt19937 rng(SEED);
    for (auto _ : state) {
        // The usual way to keep a vector of objects ordered: erase in place, shifting everything after the hole.
        for (std::size_t i = 0; i < ERASE_COUNT; ++i) {
            auto const it = world.objects.begin() + static_cast<std::ptrdiff_t>(rng() % world.objects.size());
            erased.push_back(std::move(*it));  // kept alive: the deallocation is not part of the erase cost
            world.objects.erase(it);
        }
        benchmark::DoNotOptimize(world.objects.data());
        state.PauseTiming();
        for (auto& object : erased) {
            world.objects.push_back(std::move(object));
        }
        erased.clear();
        state.ResumeTiming();
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * ERASE_COUNT));
}

// --- Insertion: fill an empty container ---------------------------------------------------------------------------

void insertSparseSet(benchmark::State& state) {
    std::vector<ecs::Entity> const order = shuffledEntities(sizeOf(state));
    for (auto _ : state) {
        ecs::SparseArray<Position> array;
        for (ecs::Entity const e : order) {
            array.insertAt(e, Position{.x = 0.0F, .y = 0.0F});
        }
        benchmark::DoNotOptimize(array.getDense().data());
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

void insertUnorderedMap(benchmark::State& state) {
    std::vector<ecs::Entity> const order = shuffledEntities(sizeOf(state));
    for (auto _ : state) {
        std::unordered_map<ecs::Entity, Position> map;
        for (ecs::Entity const e : order) {
            map.emplace(e, Position{.x = 0.0F, .y = 0.0F});
        }
        benchmark::DoNotOptimize(map.size());
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

void insertOop(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<std::unique_ptr<GameObject>> objects;
        for (std::size_t i = 0; i < sizeOf(state); ++i) {
            objects.push_back(std::make_unique<MovingObject>(static_cast<float>(i)));
        }
        benchmark::DoNotOptimize(objects.data());
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

/** @brief Container sizes: 1k, 10k and 100k elements. */
void sizes(benchmark::Benchmark* bench) { bench->RangeMultiplier(10)->Range(1'000, 100'000); }

}  // namespace

BENCHMARK(iterateSparseSet)->Apply(sizes);
BENCHMARK(iterateSparseSetOneComponent)->Apply(sizes);
BENCHMARK(iterateUnorderedMap)->Apply(sizes);
BENCHMARK(iterateOop)->Apply(sizes);
BENCHMARK(eraseSparseSet)->Apply(sizes);
BENCHMARK(eraseUnorderedMap)->Apply(sizes);
BENCHMARK(eraseOop)->Apply(sizes);
BENCHMARK(insertSparseSet)->Apply(sizes);
BENCHMARK(insertUnorderedMap)->Apply(sizes);
BENCHMARK(insertOop)->Apply(sizes);

BENCHMARK_MAIN();
