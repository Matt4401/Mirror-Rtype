# Prefabs

## Purpose

A prefab is a named entity template: a factory that attaches a pre-configured set of components to a freshly spawned entity. The generic `prefabs::PrefabManager` lives in the engine (`engine/src/prefabs/`, target `engine_prefabs`). Concrete prefabs depend on the game, so they live in the game server (`server/src/prefabs/`, target `rtype_prefabs`): each game ships its own set.

## `prefabs::PrefabManager`

| Method | Description |
|--------|-------------|
| `registerPrefab(name, factory, defaults = {})` | Registers a factory under `name`, replacing any previous one. `defaults` are parameters applied to every spawn of this name. |
| `has(name)` | Returns whether a factory exists for `name`. |
| `spawn(name, buffer, params = {})` | Queues the creation of an instance into an `ecs::CommandBuffer`. `params` are merged over the registration defaults. Returns `false` and logs a warning if `name` is unknown. |

A factory has the signature:

```cpp
using Factory = std::function<void(ecs::Entity, ecs::Registry&, PrefabParams const&)>;
```

!!! note "Deferred spawning"
    `spawn` never touches the `Registry` directly. The factory is queued with `CommandBuffer::deferSpawn` and only runs on the next `CommandBuffer::flush`. Prefabs can therefore be requested from inside a system loop without invalidating sparse array iterators.

## `prefabs::PrefabParams`

A key/value bag of spawn parameters (`engine/src/prefabs/PrefabParams.hpp`). A prefab reads the keys it understands and uses its own default for every missing key, so prefabs can take any number of parameters (position, rotation, speed, size...) without changing the `PrefabManager` API.

| Method | Description |
|--------|-------------|
| `PrefabParams{{"key", value}, ...}` | Builds the bag from a list of pairs. |
| `set(key, value)` | Sets or replaces a key; returns the bag so calls can be chained. |
| `has(key)` | Returns whether the key exists. |
| `find<T>(key)` | Returns the value as `std::optional<T>`; `std::nullopt` if missing or of an incompatible type. |
| `get(key, fallback)` | Returns the value, or `fallback` if missing or of an incompatible type. |
| `merge(overrides)` | Copies every key of `overrides`, replacing existing ones. |

Values are stored as `bool`, `std::int64_t`, `double` or `std::string`. Numbers convert between each other (`{"halfHeight", 8}` is read fine as a `float`); `bool` and strings only match themselves.

`makeTransform(params)` builds an `ecs::Transform` from the standard keys `x`, `y`, `z`, `rotation` (radians), `scaleX` and `scaleY`. Every built-in prefab uses it.

## Usage

```cpp
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabRegistry.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"

prefabs::PrefabManager manager;
prefabs::registerRtypePrefabs(manager);  // every R-Type prefab under its default name

// Variants: same factory, other defaults
manager.registerPrefab("fast_missile", prefabs::buildMissile, {{"speed", -1200.0F}});
manager.registerPrefab("obstacle_wide", prefabs::buildObstacle, {{"halfWidth", 128.0F}, {"halfHeight", 16.0F}});

ecs::CommandBuffer buffer;
manager.spawn("missile", buffer, {{"x", 800.0F}, {"y", 300.0F}, {"rotation", 0.3F}, {"speed", -900.0F}});
manager.spawn("obstacle_wide", buffer, {{"x", 600.0F}, {"y", 400.0F}});
buffer.flush(registry);  // the entities are created here
```

Every built-in prefab exposes a single `buildXxx(entity, registry, params)` function, registered directly with `registerPrefab`. Adding a parameter to a prefab only means reading one more key in its `buildXxx`.

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
