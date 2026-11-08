# ECS Benchmark

[Algorithms](algorithms.md) explains why the engine stores components in sparse sets. This page measures it: the same
data and the same work, stored three ways.

- **Sparse set**: the engine's `ecs::SparseArray`, iterated with `Registry::runSystem`.
- **`std::unordered_map`**: one `std::unordered_map<Entity, Component>` per component type.
- **OOP**: a `std::vector<std::unique_ptr<GameObject>>`, where each object holds its position, its velocity and 48 bytes
  of other state, and has a virtual `update()`. The objects are allocated between unrelated allocations, and the
  vector is shuffled, as a real game spreads its objects over the heap.

The code is `tests/ecs/SparseArrayBenchmark.cpp` (Google Benchmark).

## Protocol

| | |
|---|---|
| CPU | Intel Core i7-1355U, 48 KiB L1d, 1.25 MiB L2 per core, 12 MiB L3 |
| System | Arch Linux, kernel 7.2.14 |
| Compiler | GCC 16.2.1, `-O3 -DNDEBUG -std=c++23` (`xmake f -m release`) |
| Library | Google Benchmark 1.9.5 |
| Runs | 10 repetitions, median shown, pinned to one core. The variation between runs is under 3 %, except OOP iteration at 100,000 (7.5 %). |

```sh
xmake f -m release --benchmarks=y && xmake build bench_ecs
taskset -c 2 build/linux/x86_64/release/bench_ecs --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
```

The CPU governor was `powersave`, as on most laptops. The numbers compare the three storages on one machine. They are
not absolute values to quote for another one.

## Iteration

Add the velocity of every entity to its position. Millions of entities per second, higher is better.

| Entities | Sparse set, `runSystem<Position, Velocity>` | Sparse set, one component | `unordered_map` | OOP |
|---|---|---|---|---|
| 1,000 | 434 | 4,220 | 349 | **931** |
| 10,000 | 425 | 4,147 | 270 | **506** |
| 100,000 | **426** | 2,833 | 131 | 82 |

- **The sparse set keeps its speed as the world grows.** It reads dense arrays from front to back, which the CPU
  prefetches, so its throughput is the same from 1,000 to 100,000 entities. The OOP design drops by a factor of 11: at
  100,000 objects (about 20 MB with the allocations around them), the objects no longer fit in the caches, and each
  pointer leads to a cache miss. The sparse set is then **5 times faster** than OOP, and 3 times faster than `unordered_map`.
- **OOP is faster on small worlds.** 1,000 objects fit in L2 and 10,000 in L3, so a pointer jump costs less than the
  work `runSystem` does for a second component: for each entity of the first array, it checks that the second
  array has it and fetches it through its sparse vector. That lookup costs about 2 ns per entity, at any size.
- **Data read together should live in one component.** With position and velocity in a single component, iteration is
  only a walk over one dense array: 7 to 10 times faster than the two-component join, and 4.5 to 34 times faster than OOP.

## Erase

Erase 1,000 random elements from a container of the given size. Nanoseconds per erase, lower is better.

| Elements | Sparse set (swap-and-pop) | `unordered_map` | OOP (`vector::erase`) |
|---|---|---|---|
| 1,000 | **2.0** | 12.9 | 99 |
| 10,000 | **2.7** | 14.3 | 1,873 |
| 100,000 | **2.9** | 15.3 | 19,398 |

Swap-and-pop is $O(1)$: 100 times more elements make an erase 1.5 times slower, which is the cache, not the algorithm.
`vector::erase` is $O(n)$: it shifts every element after the hole, and its cost grows with the size. This is the usual
way to remove an object while keeping the order of a vector.

!!! note "This comparison favours the OOP design"
    The OOP benchmark is given the position of the object to remove. A real game would first have to find it, which
    is another $O(n)$ search. The sparse set finds it in $O(1)$ through its sparse vector.

## Insertion

Fill an empty container. Nanoseconds per insertion, lower is better.

| Elements | Sparse set | `unordered_map` | OOP |
|---|---|---|---|
| 1,000 | **2.8** | 32.7 | 23.5 |
| 10,000 | **3.1** | 34.6 | 22.7 |
| 100,000 | **4.4** | 49.9 | 22.6 |

The sparse set appends to vectors, which grow by doubling. The two others allocate a node or an object for each
element.

## Memory checks

The ECS suites (`test_ecs`, `test_ecs_stress`) run in CI under AddressSanitizer, LeakSanitizer and UBSan, then under
Valgrind (`ecs-memory` job of `.github/workflows/build.yml`). The stress tests drive 50,000 entities through spawns,
destroys and swap-and-pops: see [Test Audit](../development/test-audit.md).

Locally, under ASan, LeakSanitizer and UBSan, the ECS, prefab and scripting suites report no error and no leak. Running
them found one use-after-free in `CommandBuffer::flush`, now fixed.

## Conclusion

The measures back the choice of a sparse set ECS, with one nuance:

- Iteration does not degrade with the number of entities, while OOP falls off once the world leaves the CPU caches.
  On small worlds OOP is faster than a two-component `runSystem`, because of the lookup in the second array.
- Erasing is $O(1)$ and costs a few nanoseconds at any size, which matters for a shoot'em up that creates and destroys
  bullets every frame.
- Insertion is the fastest of the three.

The lookup in the second array is the main cost of `runSystem`. Grouping data used together into one component is the
cheapest way to avoid it. `runSystem` itself could also skip its second lookup: today it calls `contains()` and then
`get()` for each non-driver component, which looks the entity up twice.
