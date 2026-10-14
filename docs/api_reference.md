# C++ API Reference

Welcome to the automated C++ API Reference for the R-Type engine and subsystems.

The documentation in this section is **automatically extracted from code docstrings** using [Doxygen](https://www.doxygen.nl/index.html) and generated into MkDocs Material via [MkDoxy](https://github.com/JakubAndrysek/MkDoxy).

---

## Quick Navigation

* **[Class Index](rtype/classes.md)**: Alphabetical list of all classes, structs, and interfaces.
* **[Namespaces](rtype/namespaces.md)**: Index of all engine namespaces (`ecs`, `scripting`, `systems`).
* **[Class Hierarchy](rtype/hierarchy.md)**: Inheritance tree for classes and base abstractions.
* **[Files & Headers](rtype/files.md)**: Header files and source documentation.

---

## Core Subsystems

### 1. Scripting Subsystem (`scripting` & `systems`)

The scripting module embeds the Wren lightweight VM into the C++23 engine, providing safe, high-performance scripting for gameplay designers without macro dependencies.

| Class / Struct | Role & Description |
| :--- | :--- |
| [`scripting::WrenVM`](rtype/classscripting_1_1WrenVM.md) | High-level C++23 RAII facade managing the Wren VM instance, template slots (`setSlot<T>`, `getSlot<T>`), and `std::expected` interpretation. |
| [`scripting::ComponentBinder<T>`](rtype/classscripting_1_1ComponentBinder.md) | Type-safe fluent builder binding C++ ECS component properties to Wren foreign classes using compile-time pointer-to-members (NTTP). |
| [`scripting::ComponentBinderBase`](rtype/classscripting_1_1ComponentBinderBase.md) | Non-template base managing foreign class allocation, property storage, and Wren script generation. |
| [`systems::ScriptSystem`](rtype/classsystems_1_1ScriptSystem.md) | ECS system managing script lifecycles (`onSpawn`), timed coroutines (`Engine.wait`), opt-in frame updates (`update`), and event forwarding. |
| [`systems::SleepingFiber`](rtype/structsystems_1_1SleepingFiber.md) | Holds a suspended Wren Fiber and countdown timer for non-blocking coroutines. |
| [`ecs::ScriptComponent`](rtype/structecs_1_1ScriptComponent.md) | ECS component attaching a Wren script class instance to an entity. |
| [`scripting::detail::PropertyAccessor`](rtype/structscripting_1_1detail_1_1PropertyAccessor.md) | Internal FFI template helper converting C++ struct fields to/from Wren VM slots. |
| [`scripting::detail::UniqueWrenHandle`](rtype/namespacescripting_1_1detail.md) | RAII `std::unique_ptr` ensuring automatic release of Wren VM handles. |

---

### 2. Entity Component System (`ecs`)

High-performance data-oriented ECS architecture utilizing sparse sets and deferred mutations.

| Class / Struct | Role & Description |
| :--- | :--- |
| [`ecs::Registry`](rtype/classecs_1_1Registry.md) | Central ECS coordinator managing entity lifecycles, component sparse arrays, and views. |
| [`ecs::CommandBuffer`](rtype/classecs_1_1CommandBuffer.md) | Deferred command buffer for safe structural mutations (`deferSpawn`, `deferDestroy`, `flush`) during iteration. |
| [`ecs::SparseArray<T>`](rtype/classecs_1_1SparseArray.md) | Contiguous packed storage indexed via sparse entity IDs for cache-friendly iteration. |
| [`ecs::EventBus`](rtype/classecs_1_1EventBus.md) | Decoupled publish/subscribe event dispatcher connecting systems and scripts. |

---

## How to Document New Code

All classes, methods, and structures must include Doxygen docstrings (`/** ... */`) in their `.hpp` headers. MkDoxy automatically parses these tags upon running `mkdocs build`.

```cpp
/**
 * @brief Brief one-line summary of what the class/function does.
 *
 * Detailed explanation of algorithms, memory ownership, or invariants.
 *
 * @tparam T Description of template parameter.
 * @param arg Description of parameter.
 * @return Description of return value.
 */
```
