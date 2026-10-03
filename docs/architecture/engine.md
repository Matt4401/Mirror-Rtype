# Game Engine (ECS)

## 1. Introduction

The R-Type Game Engine is built as a generic, reusable, and modular standalone library. It is strictly decoupled from the specific gameplay rules and assets of R-Type. This modularity ensures that the engine can power different types of games (such as Pong or other custom titles) to demonstrate true reusability as required by Track #1.

## 2. Core Paradigm: Entity-Component-System (ECS)

To maximize performance, flexibility, and architectural decoupling, the engine is designed around a custom **Entity-Component-System (ECS)** architecture (Data-Oriented Design).

### 2.1 Entities

An **Entity** is not an object containing behaviors. It is simply a lightweight, unique identifier (e.g., `uint32_t` or `size_t`). Entities serve as logical handles to associate components together.

### 2.2 Components

**Components** are Plain Old Data (POD) structures containing state only and no business logic or methods.

- `Transform`: Position (`x`, `y`), rotation, and scale.
- `Velocity`: Directional speed (`dx`, `dy`).
- `Renderable`: Sprite ID, color tint, animation frame.
- `Collider`: Bounding box or radius for hit testing.
- `Health`: Current and maximum hit points.

### 2.3 Systems

**Systems** hold all game logic and behavior. Systems do not target concrete game concepts (e.g. "Spaceship" or "Monster"), but rather process entities matching a specific set of components:

- **MovementSystem:** Updates `Transform` positions based on `Velocity` and delta time.
- **RenderSystem:** Queries entities having `Transform` and `Renderable` components to draw them via the graphics backend.
- **CollisionSystem:** Evaluates overlap between entities with `Transform` and `Collider`.

### 2.4 The Registry & Sparse Sets

The **Registry** coordinates the entire ECS lifecycle:

- Entity allocation, recycling, and destruction.
- Component storage organized using **Sparse Sets** for each component type, ensuring contiguous memory layout in cache and $O(1)$ lookups/insertions/removals.
- Filtering and view iteration for systems.

```mermaid
graph TD
    subgraph Registry["Engine Core (Registry)"]
        E1["Entity 1"] --> C1["Transform"]
        E1 --> C2["Velocity"]
        E2["Entity 2"] --> C3["Transform"]
        E2 --> C4["Renderable"]
    end

    subgraph Systems["Systems"]
        MS["Movement System"] -.->|"Queries Transform + Velocity"| E1
        RS["Render System"] -.->|"Queries Transform + Renderable"| E2
    end
```

## 3. Engine Subsystems

The engine is partitioned into decoupled subsystems:

- **ECS Core:** Manages entity lifecycles, component pools, and system execution order.
- **Rendering & Audio (Raylib):** Abstracts 2D drawing, textures, sprite batching, font rendering, and sound effects.
- **Physics & Collisions:** Manages spatial partitioning (e.g., Quadtree or Grid) and AABB collision queries.
- **Event Bus:** Decoupled publisher/subscriber messaging mechanism enabling asynchronous events (e.g., audio triggers on collision without direct coupling).
- **Network Abstraction (Asio):** Transport layer supporting both TCP (lobby, auth) and UDP (real-time gameplay state synchronization).

## 4. Decoupling & Modularity

1. **Standalone Library:** Built independently as a static or dynamic library with its own build targets.
2. **Game Agnostic:** Does not contain any R-Type specific references (no hardcoded player, enemy, or bullet logic).
3. **Pluggable Systems:** Games register their own domain-specific components and systems into the engine's `Registry`.
