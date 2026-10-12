<!-- markdownlint-disable MD026 -->
# Comparative Study: Event Systems & Scripting Architecture

## 1. Introduction

As our custom engine relies on a strict, data-oriented Entity-Component-System (ECS) architecture, managing communication between isolated systems is critical. We use an `EventBus` to decouple systems, allowing them to communicate asynchronously.

However, integrating a dynamic scripting language (like Wren or Lua) introduces a fundamental architectural challenge: **How do static C++ engine events communicate with dynamic runtime scripts without destroying performance?**

This document analyzes different architectural approaches to bridging C++ ECS events with a scripting runtime and justifies our chosen solution.

## 2. The Core Problem: Static vs. Dynamic Types

C++ is a statically typed, compiled language. Our C++ `EventBus` utilizes templates (`EventBus<T>`) to achieve maximum performance. The compiler generates unique memory layouts and dispatch functions for every specific event type at compile time.

Scripting languages, conversely, are dynamically typed and create objects at runtime. **It is physically impossible for a script to instantiate a new C++ type template (`EventBus<NewScriptEvent>`) at runtime.** Therefore, a bridge is required.

## 3. Comparative Analysis of Architectures

### 3.1. The "Generic Payload" Bus (Unified Variant Bus)

In this approach, the C++ engine exposes a single, generic event structure that both C++ systems and scripts use.

```cpp
struct ScriptEvent {
    size_t id;       // Hashed string ID
    std::any payload; // Variant or JSON-like dictionary
};
```

* **Pros:** Easy to implement. Scripts can define and emit any arbitrary event on the fly.
* **Cons:** Extremely poor performance. Every event dispatch requires a hash lookup (switch-case on `id`) and an unsafe type-cast (`std::any_cast`) or payload parsing. It defeats the purpose of a fast, data-oriented C++ ECS.
* **Verdict:** Rejected. It sacrifices the core engine's performance for scripting convenience.

### 3.2. Code Generation / Pre-compilation

This approach parses scripts before C++ compilation and generates native C++ structs for every script event.

* **Pros:** Native C++ performance. 100% type safety.
* **Cons:** Destroys the primary benefit of scripting: rapid iteration. Adding a new gameplay event requires recompiling the engine or dynamically reloading shared libraries.
* **Verdict:** Rejected. We want Game Designers to iterate on scripts dynamically at runtime.

### 3.3. The "Dual Bus" Architecture (Separation of Concerns)

This approach completely separates the engine's internal events from the scripting language's events.

1.**C++ EventBus:** Strictly compile-time, strongly typed, and used solely for core engine mechanics (e.g., `PhysicsCollisionEvent`, `WindowResizeEvent`).
2.**Script EventBus:** Managed entirely within the Script VM (e.g., a Wren class managing a list of callbacks). Used for high-level gameplay logic (e.g., `QuestCompletedEvent`).

Communication between the two buses is strictly prohibited. Instead, the two worlds communicate via **explicit function bindings (FFI - Foreign Function Interface)**.

* **Pros:**
  * Zero performance overhead for pure C++ systems.
  * Zero performance overhead for pure script logic.
  * Clean architectural boundaries.
* **Cons:** Requires writing explicit bindings when C++ needs to call a script function, or when a script needs to call a C++ function.

## 4. Chosen Architecture: Dual Bus with Explicit FFI

We have chosen the **Dual Bus** architecture. To maximize the performance of our ECS, we refuse to dilute our C++ `EventBus` with generic, type-erased variants just to accommodate scripts.

If a C++ physics system needs to notify a script of a collision, it will not push an event to a shared bus. Instead, it will explicitly invoke a bound script callback: `ScriptVM::Call("onCollision", entityA, entityB)`.
Conversely, if a script needs to apply physical damage, it will call a bound C++ function `Engine::ApplyDamage(entity, amount)` rather than emitting a generic C++ event.

### 4.1. Hybrid Execution Model: Event-Driven with Selective Polling

A purely event-driven model completely avoids per-frame Foreign Function Interface (FFI) overhead. However, real-world game logic often demands continuous, per-frame calculations (e.g., homing missile trajectory steering, oscillating platform paths).

To resolve this without sacrificing performance:

1. **Event-Driven by Default:** Scripts listen to specific engine events via convention-based "magic methods" (`onCollision`, `onDamage`, `onSpawn`). No per-frame FFI cost is incurred for entities that only react to occurrences.
2. **Selective `update(dt)` Polling:** If a script explicitly defines an `update(dt)` method, the C++ `ScriptSystem` registers that specific entity into an active polling list. Only these entities incur per-frame FFI dispatch overhead, keeping thousands of purely reactive entities at zero cost.

### 4.2. C++23 Type-Safe Binding via Pointer-to-Member

Exposing ECS components to the scripting VM often degenerates into brittle C-preprocessor macros or heavy runtime reflection libraries. We leverage modern C++23 pointer-to-member templates (`ComponentBinder<T>`):

```cpp
ScriptEngine::bindComponent<Position>("Position")
    .property("x", &Position::x)
    .property("y", &Position::y);
```

* **Type Safety:** The compiler verifies member types at compile time and auto-deduces the required Wren slot conversions (e.g., float, double, integer, boolean).
* **Maintainability:** Adding or modifying a component property takes a single line in C++, completely avoiding macro expansions and tedious manual FFI getter/setter boilerplate.
* **Single Source of Truth:** Wren objects act as direct proxies to C++ memory in the ECS Sparse Sets rather than synchronizing duplicate state.

### 4.3. Structural ECS Integrity via the Command Buffer Pattern

A major vulnerability in naive scripting integrations occurs when a script mutates ECS topology directly during execution (e.g., calling `entity.destroy()` or spawning new entities while systems are iterating over Sparse Sets). In a Swap-and-Pop Sparse Set, immediate deletion alters dense array indices mid-iteration, causing iterator invalidation, skipped entities, or memory corruption.

To guarantee deterministic safety:

* Scripts never delete or instantiate entities directly.
* Script operations write to a C++ **`CommandBuffer`** (`deferDestroy`, `deferSpawn`).
* The engine flushes all structural mutations at strict frame-boundary phases, completely isolating the script execution phase from ECS topology changes.

### 4.4. Asynchronous Gameplay Sequences via Wren Fibers (Coroutines)

Complex gameplay behaviors (such as multi-phase boss patterns, cutscenes, and timed dialogue) traditionally require complex state machines and manual timer management when written in pure event-driven or update loops.

Wren provides native, stackful coroutines called **Fibers**. We bind the C++ engine clock to Wren Fibers:

* A script can invoke `Engine.wait(seconds)`.
* The VM yields control back to C++ without blocking the main engine thread.
* The C++ `ScriptSystem` manages sleeping Fibers and resumes them seamlessly when their allotted delta time has elapsed.

### 4.5. Spatial Awareness and ECS World Queries

To prevent scripts from operating in a vacuum, the C++ engine exposes high-performance spatial queries to the VM (`World.getNearestWithTag`, `World.getEntitiesInRadius`). The heavy computational lifting (iterating over Spatial Partitioning trees or Sparse Sets) remains 100% in native compiled C++, returning only the resulting Entity identifiers to Wren.

---

## 5. Industry Comparisons & Validation

1. **Unity DOTS (Data-Oriented Technology Stack):** Unity's high-performance C# ECS completely isolates its execution from the standard dynamic `MonoBehaviour` scripting environment. DOTS uses strict structs and Burst-compiled queues. Dynamic scripts cannot arbitrarily inject generic events into the DOTS pipeline; they must use explicit NativeArray buffers or tightly controlled API calls.
2. **Id Tech / Naughty Dog Custom Engines:** These highly optimized AAA engines often use strict compile-time C++ for core simulation and separate, lightweight script VMs (like custom Lisp or Scheme) that handle their own internal state and message passing, communicating with C++ only through heavily restricted, explicit bindings.
3. **EnTT (C++ ECS Library):** The author of EnTT explicitly advises against using type-erased, string-based dispatchers for performance-critical ECS communication. EnTT promotes strict typed dispatchers and recommends using its explicit reflection system (`entt::meta`) for FFI script integration, rather than unified variant buses.
4. **Godot Engine (Anti-pattern context):** Godot uses a unified Variant-based Signal system (similar to 3.1). While highly ergonomic for GDScript, this dynamic bridging is famously known to be a CPU bottleneck when hundreds of C++ nodes communicate with scripts on every frame. For high performance, Godot developers are forced to bypass the Signal system and write explicit GDExtension (C++) modules. We avoid this bottleneck natively by adopting the Dual Bus.
