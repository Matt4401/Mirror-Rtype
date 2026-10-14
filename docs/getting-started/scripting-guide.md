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
import "core" for Entity

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

Components attached to your entity are accessible as properties directly on `_entity`.

```wren
// Read component values
var currentX = _entity.position.x
var currentY = _entity.position.y

// Modify component values directly
_entity.position.x = currentX + 10.0
_entity.position.y = 200.0
```

> [!NOTE]
> Component properties are direct proxies to native engine data. When you assign `_entity.position.x = 50.0`, the C++ engine's internal sparse sets update immediately.

---

## 5. Event Callbacks ("Magic Methods")

Instead of checking states every frame, implement convention-based event methods. The engine invokes them automatically when events occur:

| Magic Method | Trigger | Parameters |
| :--- | :--- | :--- |
| `onSpawn()` | When the entity and script are initialized | None |
| `onCollision(other)` | When physics detects a collision | `other` (`Entity`) |
| `onDamage(amount)` | When this entity takes damage | `amount` (Number) |
| `update(dt)` | *(Optional)* Called every frame for continuous logic | `dt` (Delta time in seconds) |

### Example: Reactive Defense Drone

```wren
class DefenseDrone {
    construct new(entity) {
        _entity = entity
        _shield = 100
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
import "core" for Entity

class WaveMotionEnemy {
    construct new(entity) {
        _entity = entity
        _time = 0.0
    }

    update(dt) {
        _time = _time + dt
        // Move horizontally and oscillate vertically
        _entity.position.x = _entity.position.x - (80.0 * dt)
        _entity.position.y = 250.0 + (50.0 * (_time * 3.0).sin)
    }
}
```

> [!TIP]
> Only define `update(dt)` if you truly need per-frame continuous adjustments. Purely reactive entities without `update(dt)` execute with zero overhead per frame.

---

## 7. Asynchronous Sequences & Coroutines (Fibers)

For timed attack patterns, boss phases, or cinematics, you do not need complicated state machines or manual countdown timers. Use Wren **Fibers** with `Engine.wait(seconds)`:

```wren
import "core" for Entity, Engine

class BossPattern {
    construct new(entity) {
        _entity = entity
    }

    onSpawn() {
        Fiber.new {
            while (true) {
                // Phase 1: Charge forward
                _entity.position.x = _entity.position.x - 50.0
                Engine.wait(1.5) // Pause script for 1.5s without freezing the game

                // Phase 2: Rapid fire burst
                shootLaser()
                Engine.wait(0.2)
                shootLaser()
                Engine.wait(0.2)
                shootLaser()
                Engine.wait(2.0)

                // Phase 3: Retreat
                _entity.position.x = _entity.position.x + 50.0
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
import "core" for Entity, World

class HomingMissile {
    construct new(entity) {
        _entity = entity
        _target = null
    }

    onSpawn() {
        // Find nearest entity tagged "Player"
        _target = World.getNearestWithTag(_entity.position.x, _entity.position.y, "Player")
    }

    update(dt) {
        if (_target != null) {
            // Track towards target position
            var targetX = _target.position.x
            var targetY = _target.position.y
            if (_entity.position.y < targetY) {
                _entity.position.y = _entity.position.y + (120.0 * dt)
            } else {
                _entity.position.y = _entity.position.y - (120.0 * dt)
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
_entity.destroy()
```

To spawn an entity prefab:

```wren
Engine.spawnPrefab("Missile", _entity.position.x + 20.0, _entity.position.y)
```

Both calls are queued in the engine's `CommandBuffer` and resolved safely at the end of the frame, avoiding invalid memory operations.
