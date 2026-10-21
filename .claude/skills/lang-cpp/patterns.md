# Design patterns in modern C++

Use a pattern only if it solves a real problem; prefer language mechanisms.

| Need | Recommended approach |
|---|---|
| Manage a resource | **RAII** (guards, `unique_ptr` with a custom deleter, `std::lock_guard`) |
| Create objects by type | **Factory**: a `make_xxx()` function returning `std::unique_ptr<Interface>` |
| Build a complex object | **Builder** or an options struct with designated initialisers (C++20) |
| Vary behaviour | **Strategy** by injection: `std::function`, policy template, or virtual interface |
| Notify of changes | **Observer**: list of `std::function` callbacks, unsubscribe through an RAII object |
| Traverse a structure | **Iterators / ranges** rather than a hand-made Visitor |
| Operate on alternatives | **`std::variant` + `std::visit`** instead of a hierarchy + Visitor |
| Static polymorphism | **CRTP** or concepts (C++20) when the cost of `virtual` matters |
| Hide the implementation | **Pimpl** (`std::unique_ptr<Impl>`) to stabilise the ABI and cut compile times |
| Dependencies | **Dependency injection** through the constructor (interfaces or templates), no global singleton |
| Erase a type | **Type erasure** (`std::function`, `std::any`, custom wrapper) |

## Pitfalls
- **Singleton**: prefer an explicitly passed object; if one is needed, use a function-local static (thread-safe since C++11).
- **Virtual interfaces**: `virtual` destructor (or protected non-virtual); no copying of polymorphic objects (slicing) — go through pointer/reference or `clone()`.
- **Concurrency**: prefer tasks (`std::jthread`, `std::async`) and immutable data; protect any shared state.
