# Prefabs

## Purpose

A prefab is a named entity template: a factory that attaches a pre-configured set of components to a freshly spawned entity. Prefabs live in `engine/src/prefabs/` and are stored in a `prefabs::PrefabManager`.

## `prefabs::PrefabManager`

| Method | Description |
|--------|-------------|
| `registerPrefab(name, factory)` | Registers a factory under `name`, replacing any previous one. |
| `has(name)` | Returns whether a factory exists for `name`. |
| `spawn(name, buffer, x, y)` | Queues the creation of an instance at `(x, y)` into an `ecs::CommandBuffer`. Returns `false` and logs a warning if `name` is unknown. |

A factory has the signature:

```cpp
using Factory = std::function<void(ecs::Entity, ecs::Registry&, float x, float y)>;
```

!!! note "Deferred spawning"
    `spawn` never touches the `Registry` directly. The factory is queued with `CommandBuffer::deferSpawn` and only runs on the next `CommandBuffer::flush`. Prefabs can therefore be requested from inside a system loop without invalidating sparse array iterators.

## Usage

```cpp
#include "prefabs/PrefabManager.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"

prefabs::PrefabManager manager;
prefabs::registerMissilePrefab(manager);                              // "missile"
prefabs::registerMissilePrefab(manager, -1200.0F, "fast_missile");    // variant
prefabs::registerObstaclePrefab(manager, 128.0F, 16.0F, "obstacle_wide");

ecs::CommandBuffer buffer;
manager.spawn("missile", buffer, 800.0F, 300.0F);
buffer.flush(registry);  // the entity is created here
```

Every built-in prefab follows the same pattern:

- a `buildXxx(entity, registry, x, y, ...)` function attaching the components directly;
- a `registerXxxPrefab(manager, ..., name)` function registering a factory that calls `buildXxx` with captured parameters. Calling it several times with different names registers variants.

## Built-in prefabs

| Name | Role | Body | Collider | Category | Page |
|------|------|------|----------|----------|------|
| `missile` | Enemy projectile | Dynamic, bullet | Box 16×6, sensor | `ENEMY_SHOT` | [Missile](missile.md) |
| `player_bullet` | Player projectile | Dynamic, bullet | Box 12×4, sensor | `PLAYER_SHOT` | [Player Bullet](player-bullet.md) |
| `alien_scout` | Basic enemy | Kinematic | Box 32×32, sensor | `ENEMY` | [Alien Scout](alien-scout.md) |
| `obstacle` | Level geometry | Static | Box 64×64, solid | `OBSTACLE` | [Obstacle](obstacle.md) |
| `power_up` | Collectible bonus | Kinematic | Circle r=12, sensor | `POWER_UP` | [Power-Up](power-up.md) |

Every prefab attaches `ecs::Transform`, `ecs::Renderable`, `ecs::RigidBody2D` and `ecs::Collider2D`. Who can touch whom is described in [Collision Categories](collision-categories.md).

## Scripting

Wren scripts expose `Engine.spawnPrefab(name, x, y)` (`core/Engine` module).

!!! warning
    The binding in `ScriptSystemBindings.cpp` currently queues a placeholder spawn: it is not yet wired to `PrefabManager`, so the created entity has no components.
