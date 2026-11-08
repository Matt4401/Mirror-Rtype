# Scripting Guide (Wren)

Welcome to the R-Type engine scripting guide! Our engine embeds [Wren](https://wren.io/), a lightweight, fast, class-based concurrent scripting language.

This guide is designed for **Game Designers and scripters**. You do not need to understand C++ internals or ECS memory layouts to build gameplay features, enemy behaviors, weapons, and cutscenes.

---

## 1. Overview & Philosophy

The engine adopts a **hybrid, event-driven architecture**:

* **Zero Overhead by Default:** Scripts react to events (`onCollision`, `onDamage`) rather than running expensive loops every frame.
* **Optional Continuous Updates:** If you need continuous motion (e.g. steering a homing missile), set `hasUpdate = true` in `ScriptComponent` and define `update(dt)`. The engine only polls scripts that explicitly opt in with `hasUpdate = true`.
* **Direct Component Access:** You interact with entities and components using natural object-oriented syntax (`_entity.position.x = 100.0`).
* **Safe Mutations:** Deleting entities via `_entity.destroy()` or spawning prefabs is deferred through an engine command buffer, ensuring game stability.

---

## 2. Attaching Scripts to Entities

In C++ or level configuration files, an entity receives a script by attaching a `ScriptComponent`:

```cpp
registry.addComponent<ScriptComponent>(entity, ScriptComponent{
    .moduleName = "scripts/gameplay/HomingMissile",
    .className = "HomingMissile",
    .hasUpdate = true
});
```

The engine loads the corresponding `.wren` module and instantiates the class, passing an `Entity` handle to its constructor.

### Script root

Module names (in `ScriptComponent::moduleName` and in `import`) are paths relative to the **script root**, without
the `.wren` extension: `scripts/Boss` is the file `<root>/scripts/Boss.wren`. The root is the working directory by
default; a game sets another one with `ScriptSystem::setScriptRoot` (the R-Type server uses its data directory).

Absolute names (`/home/x`, `C:/x`) and names that climb out of the root (`../x`, `a/../../x`) are refused: the
script does not start, or the `import` fails with "Could not load module", and a warning is logged once.

---

## 3. Basic Script Structure

Every script class must define a constructor `new(entity)` accepting the entity handle:

```wren
import "core/Entity" for Entity

class BasicEnemy {
    construct new(entity) {
        _entity = entity
        _health = 50
    }

    onSpawn() {
        System.print("Enemy spawned with ID: %(_entity.id)")
    }
}
```

---

## 4. Reading and Writing Components

Components bound to the scripting engine (such as `core/Position`) can be imported and instantiated as lightweight proxies bound directly to the entity's ID:

```wren
import "core/Entity" for Entity
import "core/Position" for Position

class MovingEnemy {
    construct new(entity) {
        _entity = entity
        _pos = Position.new(entity.id)
    }

    onSpawn() {
        // Read component values
        var currentX = _pos.x
        var currentY = _pos.y

        // Modify component values directly in C++ ECS memory
        _pos.x = currentX + 10.0
        _pos.y = 200.0
    }
}
```

> [!NOTE]
> Component proxies operate directly on native C++ ECS data. When you assign `_pos.x = 50.0`, the C++ engine's internal sparse sets update immediately with zero copy or serialization overhead.

---

## 5. Event Callbacks & Lifecycle Hooks

Instead of checking states every frame, implement convention-based event methods. The engine invokes them automatically when events occur:

| Method | Trigger | Parameters | Purpose |
| :--- | :--- | :--- | :--- |
| `onSpawn()` | When entity and script are initialized | None | Initial state setup and one-time registration |
| `onReload()` | When script file is modified on disk and hot-reloaded | None | Re-initializing transient variables without losing ECS component state |
| `onDestroy()` | When entity is scheduled for deletion via command buffer | None | Spawning particle effects, audio cues, or notifying other game objects before destruction |
| `onCollision(other)` | When a physics collision begins (requires `hasCollision = true`) | `other` (`Entity`) | Reactive collision resolution |
| `onDamage(amount)` | When this entity takes damage | `amount` (Number) | Damage handling, shield reduction, visual flashing |
| `update(dt)` | *(Optional)* Called every frame for continuous logic | `dt` (Delta time in seconds) | Continuous steering, trajectories, or timers |

### Collisions: `onCollision(other)`

`ScriptPlugin` forwards every `events::CollisionBeginEvent` from the physics module to the scripts of **both** entities of the contact. A script receives it only if it opts in, like `update(dt)`:

```cpp
registry.addComponent<ScriptComponent>(entity, ScriptComponent{
    .moduleName = "scripts/gameplay/Boss",
    .className = "Boss",
    .hasCollision = true  // the class must define onCollision(other)
});
```

Wren cannot check whether a class defines a method, so the flag is required. Without it, every contact would raise a runtime error for scripts that don't handle collisions.

* `other` is the `Entity` that was hit. `other.destroy()` is deferred through the command buffer and applied at the end of the script update. It is never applied during the collision callback.
* A contact is skipped for a script when either entity is already dead, for example killed by a C++ rule that handled the same contact first.
* An entity queued for destruction is ignored until it is actually destroyed. Once the first script destroys `other`, `other`'s own `onCollision` is not called, and neither are its later contacts in the same frame. This means a projectile can't hit twice.
* Only the start of a contact is forwarded. Subscribe to `events::CollisionEndEvent` in C++ if you need the end.
* **Known limitation:** a script is instantiated during `ScriptSystem::update`, while physics runs in the fixed update. An entity that is spawned already overlapping another collider can start its contact before its script exists, and that first `onCollision` is lost. It is not replayed later. Spawn such entities away from what they should hit, or handle the case in C++.

### Example: Reactive Defense Drone with Lifecycle Hooks

```wren
import "core/Entity" for Entity

class DefenseDrone {
    construct new(entity) {
        _entity = entity
        _shield = 100
    }

    onSpawn() {
        System.print("Drone active for entity %(_entity.id)")
    }

    onReload() {
        System.print("Drone script hot-reloaded! Shield currently at %(_shield)")
    }

    onDestroy() {
        System.print("Drone %(_entity.id) destroyed! Spawning explosion...")
    }

    onCollision(other) {
        System.print("Drone collided with entity %(other.id)")
    }

    onDamage(amount) {
        _shield = _shield - amount
        if (_shield <= 0) {
            _entity.destroy()
        }
    }
}
```

---

## 6. Continuous Behavior with `update(dt)`

If your entity requires continuous per-frame logic (e.g. guiding a missile or oscillating an enemy):

```wren
import "core/Entity" for Entity
import "core/Position" for Position

class WaveMotionEnemy {
    construct new(entity) {
        _entity = entity
        _pos = Position.new(entity.id)
        _time = 0.0
    }

    update(dt) {
        _time = _time + dt
        // Move horizontally and oscillate vertically
        _pos.x = _pos.x - (80.0 * dt)
        _pos.y = 250.0 + (50.0 * (_time * 3.0).sin)
    }
}
```

> [!TIP]
> Only define `update(dt)` if you truly need per-frame continuous adjustments. Purely reactive entities without `update(dt)` execute with zero overhead per frame.

---

## 7. Asynchronous Sequences & Coroutines (Fibers)

For timed attack patterns, boss phases, or cinematics, you do not need complicated state machines or manual countdown timers. Use Wren **Fibers** with `Engine.wait(seconds)`:

```wren
import "core/Entity" for Entity
import "core/Engine" for Engine
import "core/Position" for Position

class BossPattern {
    construct new(entity) {
        _entity = entity
        _pos = Position.new(entity.id)
    }

    onSpawn() {
        Fiber.new {
            while (true) {
                // Phase 1: Charge forward
                _pos.x = _pos.x - 50.0
                Engine.wait(1.5) // Pause script for 1.5s without freezing the game

                // Phase 2: Rapid fire burst
                shootLaser()
                Engine.wait(0.2)
                shootLaser()
                Engine.wait(0.2)
                shootLaser()
                Engine.wait(2.0)

                // Phase 3: Retreat
                _pos.x = _pos.x + 50.0
                Engine.wait(1.0)
            }
        }.call()
    }

    shootLaser() {
        System.print("Boss fires a laser burst!")
    }
}
```

---

## 8. Spatial Awareness & World Queries

Scripts can query the surrounding game world using the static `World` API:

```wren
import "core/Entity" for Entity
import "core/Position" for Position
import "core/World" for World

class HomingMissile {
    construct new(entity) {
        _entity = entity
        _pos = Position.new(entity.id)
        _target = null
    }

    onSpawn() {
        // Find nearest entity tagged "Player"
        _target = World.getNearestWithTag(_pos.x, _pos.y, "Player")
    }

    update(dt) {
        if (_target != null) {
            var targetPos = Position.new(_target.id)
            if (_pos.y < targetPos.y) {
                _pos.y = _pos.y + (120.0 * dt)
            } else {
                _pos.y = _pos.y - (120.0 * dt)
            }
        }
    }

    onCollision(other) {
        _entity.destroy()
    }
}
```

---

### Reference

| Call | Returns |
| --- | --- |
| `World.getEntitiesWithTag(tag)` | `List` of `Entity` whose `Tag` equals `tag` (exact, case-sensitive) |
| `World.getNearestWithTag(x, y, tag)` | Closest tagged `Entity` that has a `Transform`, or `null` |
| `World.getEntitiesInRadius(x, y, radius)` | `List` of `Entity` whose `Transform` is within `radius` (boundary included) |

* Entities are labelled from C++ with the generic `ecs::Tag` component (`registry.addComponent<ecs::Tag>(e, {.name = "enemy"})`);
  the engine gives no meaning to the strings, each game picks its own.
* Coordinates and radius are in `Transform` world units. The queries scan the ECS directly, so they work on the
  headless server and do not need a physics body.
* `radius` is clamped to `ecs::MAX_QUERY_RADIUS`; a negative radius returns an empty list.
* No match returns an empty list or `null`, never a VM error. Entities destroyed by a script during the current
  frame are skipped. Wrong argument types abort the calling fiber.
* The same queries are available in C++ in `ecs/SpatialQueries.hpp`.

---

## 9. Entity Deletion & Prefab Spawning

To destroy an entity:

```wren
import "core/Entity" for Entity

_entity.destroy()
```

To spawn an entity prefab:

```wren
import "core/Engine" for Engine

Engine.spawnPrefab("Missile", _pos.x + 20.0, _pos.y)
```

Both calls are queued in the engine's `CommandBuffer` and resolved safely at the end of the frame, avoiding invalid memory operations or iterator corruption in active system loops.

---

## 10. Live Hot-Reloading Workflow

The engine features a non-intrusive **Live Hot-Reloading** pipeline built specifically for rapid gameplay iteration:

### 10.1 How to Hot-Reload

1. Launch the game or test harness (`r-type_client`).
2. Open any `.wren` script (e.g. `scripts/Boss.wren`) in your favorite code editor.
3. Modify logic, numbers, attack intervals, or fiber coroutines.
4. Save the file (`Ctrl+S`).
5. **Within 500 ms**, the engine automatically detects the change, reloads the script, and updates live gameplay.

### 10.2 Zero State Loss (Data-Oriented Separation)

Unlike traditional Object-Oriented engines where game state is trapped inside script instance variables (making reload prone to state corruption or requiring heavy serialization), our architecture strictly separates **State** from **Logic**:

* **All Game State** (`Position`, `Velocity`, `Health`) lives natively in C++ `SparseArray`s.
* **Wren Classes** contain pure gameplay behavior and algorithms.

When a script is hot-reloaded:

1. The engine releases stale Wren handles.
2. The VM is cleanly recreated in `< 0.2 ms`.
3. Foreign bindings and module scripts are re-registered.
4. Active `ScriptComponent`s are re-instantiated and bound to their existing entities.
5. The `onReload()` lifecycle hook is called on all active instances.
6. The entity's position, health, and physics state remain completely intact!

### 10.3 Staging VM Safety & Syntax Error Resilience

If a designer accidentally saves a script containing a syntax error or a broken import:

1. The engine compiles the modified script in an **isolated staging VM** before touching the running game.
2. If compilation fails, the error is logged to the console with the file name and line number.
3. The engine **aborts the reload** and keeps running the previous stable version without crashing or interrupting the game session.
4. Once the designer fixes the typo and saves again, the valid script is reloaded immediately.

### 10.4 Runtime Errors

A runtime error (`Fiber.abort`, a call on `null`, ...) in a hook stops that call only. It is logged through the
engine logger (`[script]` tag) on one line, with its stack trace:

```text
WARN  [script] entity 42: update(_) failed: Runtime Error: Null does not implement 'x' | in scripts/Boss line 31 in update(_)
```

* The same script logs at most **one error per second**; the next message says how many similar errors were hidden.
* After **30 failures in a row** (`MAX_CONSECUTIVE_SCRIPT_FAILURES`), the script of that entity is **disabled**: no
  more `update(dt)`, `onCollision(other)` or `Engine.wait` resumes (its `onDestroy()` still runs). This is logged
  once. A successful call resets the count, so a script failing now and then keeps running.
* The next hot reload enables every disabled script again. `ScriptSystem::isScriptDisabled(entity)` tells the state.
* `System.print` goes to the engine logger too, one `info` line per printed line.

## 11. Scenes from Wren

With `scene::SceneScriptingPlugin` loaded, scripts can change the scene stack and whole scenes can be written in
Wren. See [Scenes](../architecture/scene.md#scenes-in-wren) for the details.

```wren
import "core/Scene" for Scene

Scene.push("Pause")          // also switchTo(name), replace(name), pop()
if (Scene.current == "Game") { ... }
```
