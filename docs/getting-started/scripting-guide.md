# Scripting Guide (Wren)

Welcome to the R-Type engine scripting guide! Our engine embeds [Wren](https://wren.io/), a lightweight, fast, class-based concurrent scripting language.

This guide is designed for **Game Designers and scripters**. You do not need to understand C++ internals or ECS memory layouts to build gameplay features, enemy behaviors, weapons, and cutscenes.

---

## 1. Overview & Philosophy

The engine adopts a **hybrid, event-driven architecture**:

* **Zero Overhead by Default:** Scripts react to events (`onCollision`, `onDamage`) rather than running expensive loops every frame.
* **Optional Continuous Updates:** If you need continuous motion (e.g. steering a homing missile), define `update(dt)`. The engine only polls scripts that explicitly implement this method.
* **Direct Component Access:** You interact with entities and components using natural object-oriented syntax (`_entity.position.x = 100.0`).
* **Safe Mutations:** Deleting entities via `_entity.destroy()` or spawning prefabs is deferred through an engine command buffer, ensuring game stability.

---

## 2. Attaching Scripts to Entities

In C++ or level configuration files, an entity receives a script by attaching a `ScriptComponent`:

```cpp
registry.addComponent<ScriptComponent>(entity, ScriptComponent{
    .moduleName = "scripts/gameplay/HomingMissile",
    .className = "HomingMissile"
});
```

The engine loads the corresponding `.wren` module and instantiates the class, passing an `Entity` handle to its constructor.

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
| `onCollision(other)` | When physics detects a collision | `other` (`Entity`) | Reactive collision resolution |
| `onDamage(amount)` | When this entity takes damage | `amount` (Number) | Damage handling, shield reduction, visual flashing |
| `update(dt)` | *(Optional)* Called every frame for continuous logic | `dt` (Delta time in seconds) | Continuous steering, trajectories, or timers |

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
