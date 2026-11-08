# Test Audit

This page takes stock of the automated tests, and checks the ECS core against
[issue #40](https://github.com/Matt4401/Mirror-Rtype/issues/40): benchmarks, stress tests under heavy `CommandBuffer`
mutations, entity recycling, and memory checks in CI.

## Inventory

30 GoogleTest binaries, 569 test cases, declared in `tests/xmake.lua`, all run by `xmake test`.

| Area | Suites | Cases |
|---|---|---|
| ECS core | `test_ecs`, `test_ecs_stress` | 33 + 8 |
| Engine core | `test_engine`, `test_mediator`, `test_plugin_graph`, `test_clock`, `test_components`, `test_renderable`, `test_hierarchy`, `test_config`, `test_log` | 97 |
| Scripting and prefabs | `test_scripting`, `test_spatial_queries`, `test_prefabs` | 58 |
| Physics and collisions | `test_physics`, `test_collision_router`, `test_client_collisions` | 29 |
| Input | `test_input` | 21 |
| UI | `test_ui_components` | 49 |
| Render, audio (`graphics` only) | `test_render`, `test_audio`, `test_audio_system`, `test_audio_settings`, `test_audio_plugin` | 81 |
| Network and protocol | `test_network_components`, `test_network_plugin`, `test_transport`, `test_replication`, `test_protocol`, `test_client_network` | 106 |
| Server and game | `test_server_room`, `test_server_gameplay`, `test_server_manager`, `test_server_e2e`, `test_rtype_game` | 87 |

The `bench_ecs` binary is not a test: it measures and asserts nothing. See [ECS Benchmark](../comparative-study/ecs-benchmark.md).

## ECS core against issue #40

### What already existed

`tests/test_ecs.cpp` already covered the behaviour of one entity at a time:

- a killed slot is reused with a new generation, and the generation wraps around;
- a stale handle (same slot, older generation) can't read, write or kill the slot's new occupant;
- swap-and-pop keeps the data of the moved entity, on 3 entities;
- `CommandBuffer` destroys an entity queued three times only once;
- listeners added or removed while the registry dispatches.

These tests were kept as they are. The new suite only adds what they did not reach: scale, random orders and the
invariants of the containers.

### What was added

`tests/ecs/`, target `test_ecs_stress`:

| Test | What it proves |
|---|---|
| `CommandBufferStress.ChurnDuringSystemRunsKeepsEveryEntityConsistent` | 50,000 entities, 20 frames. During each `runSystem`, about 30 % of the entities are destroyed and as many spawned, through the buffer. Checked against a reference model: no destroyed entity is ever visited, every live entity is visited once per frame, and its data followed it through every swap-and-pop. |
| `CommandBufferStress.CommandsQueuedDuringFlushRunAtTheNextFlush` | An initializer queues 1,000 spawns and a destroy into the buffer being flushed. |
| `CommandBufferStress.DestroyListenerQueuingDuringFlushRunsAtTheNextFlush` | A destroy listener cascades to 1,000 children through the buffer being flushed. |
| `CommandBufferStress.MassDestroyOfStaleAndDuplicateHandlesOnlyKillsLiveTargets` | 50,000 destroys mixing stale handles whose slots were refilled, triplicates, `NULL_ENTITY` and a never-spawned handle: only the live targets die, and each freed slot is handed out once. |
| `EntityRecycle.MassRecyclingNeverRevivesAStaleHandle` | 50,000 random spawns and kills: no handle is handed out twice, no slot holds two live entities, every killed handle stays dead, and the registry never uses more slots than its peak of live entities. |
| `EntityRecycle.SparseArrayStaysConsistentUnderRandomInsertAndErase` | 50,000 random inserts and erases against a reference map: the sparse vector always leads to the exact dense slot. |
| `EntityRecycle.EmptyOrSparseSlotsAreReportedMissing` | Lookups in an empty array, in a hole of the sparse vector and past its end miss without reading out of bounds. |
| `EntityRecycle.SwapAndPopMovesOnlyTheLastComponent` | Each erase moves exactly one element, the last one into the hole, so it is $O(1)$. The timing proof is in the benchmark. |

The seeds are fixed, so any failure can be replayed.

### Acceptance criteria

| Criterion | State |
|---|---|
| Benchmark report showing cache iteration superiority over OOP | Done, with a nuance: the sparse set wins once the data no longer fits in the CPU caches. See [ECS Benchmark](../comparative-study/ecs-benchmark.md). |
| Zero leaks with Valgrind and AddressSanitizer | ASan, LeakSanitizer and UBSan: 0 error and 0 leak on the ECS suites, and on the whole suite except one test harness issue (see below). Valgrind runs in CI only, as it is not installed on the machine used for this audit. |
| Zero iterator invalidation or crash under heavy `CommandBuffer` mutations | One bug found and fixed (see below). The stress tests pass under ASan. |
| Fully automated in CI | The `ecs-memory` job of `.github/workflows/build.yml` runs ASan and UBSan, Valgrind, and the benchmark once. `test_ecs_stress` also runs in the Linux, Windows and macOS jobs, like every other suite. |
| Bounds checks on uninitialized `SparseArray` | Covered by `EmptyOrSparseSlotsAreReportedMissing`. |
| Entity count bounded by the entity index | `spawnEntity` throws `std::length_error` past `MAX_ENTITY_INDEX`. Not tested: reaching it takes 4 billion spawns. |

## Findings

### Fixed: `CommandBuffer::flush` used freed memory

`flush` walked its spawn queue with a range `for`. If an initializer queued a spawn into the same buffer, the vector
reallocated under the loop, and the next initializer was called from freed memory. ASan reported a
`heap-use-after-free` in `CommandBuffer::flush`. A destroy listener queuing destroys during the flush had the same
problem with the destroy queue, and its commands were silently dropped by the final `clear()`.

`flush` now moves both queues out before walking them. Commands queued during a flush wait for the next flush, which
the documentation of `CommandBuffer::flush` now states. This changes one behaviour: a destroy queued by an initializer
used to apply in the same flush.

### Contract: no structural change inside `runSystem`

`Registry::runSystem` reads the size of the arrays once, before the loop. Killing an entity or removing one of the
iterated components directly from the callback swaps the last element into the hole: the moved entity is skipped, and
the loop reads past the end of the array. This is by design: structural changes inside a system go through a
`CommandBuffer`, which is what the stress tests check. Nothing was changed, but `runSystem`'s documentation should say
it.

### Test harness: `test_physics` under ASan

`tests/physics/PhysicsSystemTests.cpp` replaces the global `operator new` and `operator delete` to count allocations,
but not the `std::nothrow` overloads. GoogleTest allocates with `new (std::nothrow)` and the test frees with
`std::free`, which ASan reports as an `alloc-dealloc-mismatch`. Outside ASan both end up in `malloc` and `free`, so
there is no effect at runtime. The fix is to also replace the `nothrow` overloads. The physics engine itself is not
involved.

### Smaller points

- `tests/test_ecs.cpp` defines `main()` in the middle of the file, with tests after it.
- `ECS.HighThroughputIteration` checks one pass over 10,000 entities for correctness but measures nothing. The
  benchmark now does the measuring.
- The sanitizers only run on the ECS suites in CI. Running the whole suite under ASan locally takes 35 s, so it could
  join the CI job once the physics harness is fixed.

## Running the tests

```sh
xmake f --tests=y && xmake && xmake test                        # every suite
xmake run test_ecs_stress                                       # one suite
xmake run test_ecs_stress --gtest_filter='CommandBufferStress.*'

# Under ASan and UBSan (only our targets are instrumented, not the packages)
xmake f -m debug --graphics=n --tests=y --sanitizers=y && xmake build test_ecs_stress && xmake run test_ecs_stress

# Under Valgrind (without the sanitizers)
xmake f -m debug --graphics=n --tests=y --sanitizers=n && xmake build test_ecs_stress
valgrind --leak-check=full --error-exitcode=1 build/linux/x86_64/debug/test_ecs_stress
```

!!! warning "Give the sanitizers through the option"
    `xmake f --policies=build.sanitizer.address` also applies to the packages, which xmake then rebuilds with the
    sanitizers (and Wren fails to build that way). Use `--sanitizers=y`, which sets the policies inside the project.
