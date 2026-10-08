<!-- markdownlint-disable MD024 -->
# Algorithms & Data Structures: Architecture Choices

This document compares our core algorithms and data structures with their main alternatives.

---

## 1. Benchmark: Core Engine Paradigm

The engine requires a paradigm that can handle thousands of dynamic objects efficiently, without the spaghetti code associated with deep inheritance trees.

| Criterion | Pure OOP (Inheritance) | Archetype-based ECS (Unity DOTS) | **Sparse Set ECS (Our Choice)** |
| :--- | :--- | :--- | :--- |
| **Cache Locality** | ✗ Terrible (Pointer chasing) | ✓ Excellent (Contiguous blocks) | ✓ Excellent (Dense arrays) |
| **Flexibility** | ✗ Diamond problem | ✓ Highly flexible | ✓ Highly flexible |
| **Adding/Removing Components** | ✓ Fast | ✗ Very slow (Moves entities across blocks) | ✓ Very fast ($O(1)$ Swap & Pop) |
| **Implementation Complexity** | Low | Extremely High | Medium |
| **Query Iteration Speed** | Slow | Maximum | Very Fast |

### Analysis

- **Pure OOP** suffers from the "Diamond Problem" (e.g., a `Player` inheriting from `Destructible` and `Movable`). It also scatters data across the heap, causing CPU cache misses.
- **Archetype ECS** groups entities with exact matching components into the same memory blocks. It offers the fastest iteration speed but is extremely complex to implement from scratch. Furthermore, adding or removing a component at runtime forces the engine to move the entire entity to a new memory block, causing severe performance drops for highly dynamic games.
- **Sparse Set ECS** (Our choice) maps entity IDs to contiguous dense arrays using a sparse array. Iteration is incredibly fast, and adding or removing a component is an $O(1)$ operation thanks to the **Swap & Pop** idiom.

**Decision: Sparse Set ECS.** It perfectly balances extreme performance, modularity, and implementability for the scope of this project.

---

## 2. Benchmark: System Iteration & Filtering

How do systems interact with the data stored in the ECS?

| Criterion | Virtual Inheritance (`ISystem`) | Bitmask / Tag Matching | **Variadic Templates (Our Choice)** |
| :--- | :--- | :--- | :--- |
| **Performance Overhead** | ✗ High (vtable lookups) | Medium (Bitwise checks) | ✓ Zero-cost (Compile-time) |
| **Type Safety** | Low (Dynamic casts) | Low | ✓ High (Strict typing) |
| **Readability** | Verbose | Verbose | ✓ Clean and modern |

### Analysis

- **Virtual Inheritance** forces systems to override an `update()` method and cast generic `IComponent*` back to concrete types, which is slow and unsafe.
- **Bitmask Matching** requires iterating over all entities and checking if their bitmask matches the system's required components, which breaks branch prediction.
- **Variadic Templates** (`registry.run_system<A, B>(...)`) use C++17 fold expressions. The compiler perfectly resolves the types at compile-time, resulting in aggressively inlined, zero-cost iteration loops.

**Decision: Variadic Templates.** It leverages modern C++23 features for unbeatable performance and developer experience.

---

## 3. Benchmark: Inter-System Communication

How do decoupled systems communicate (e.g., the Physics system notifying the Audio system of a collision)?

| Criterion | Direct Method Calls | `std::any` Event Callbacks | **Type-Erased `std::move_only_function`** |
| :--- | :--- | :--- | :--- |
| **Decoupling** | ✗ Zero | ✓ Total | ✓ Total |
| **Memory Allocation** | Zero | ✗ High (Heap allocations) | ✓ Zero |

### Analysis

Our **Event Bus** requires a publisher/subscriber model. Standard type-erasure often relies on `std::any` or `std::function`, which trigger heap allocations (unless small buffer optimization saves them). By using C++23's `std::move_only_function` and casting void pointers internally, we achieve a totally decoupled Event Bus that guarantees zero dynamic allocations during event broadcasting.

**Decision: Type-Erased Event Bus.** Ensures modularity without sacrificing real-time performance.
