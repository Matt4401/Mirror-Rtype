# Power-Up

**Name:** `power_up` (`prefabs::POWER_UP_PREFAB`)
**Source:** `server/src/prefabs/power_up/`

## Purpose

A collectible bonus slowly drifting to the left with the scrolling. Its collider is a sensor: it detects the player without physically blocking it.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Kinematic`, `velocityX = speed` |
| `Collider2D` | Circle, `radius = POWER_UP_RADIUS`, sensor |

## Collision

| Category | Mask |
|----------|------|
| `POWER_UP` | `POWER_UP_MASK` = `PLAYER` |

Only players can pick it up. Shots, enemies and obstacles go through it. See [Collision Categories](collision-categories.md).

## Parameters

Every transform key read by `makeTransform` (`x`, `y`, `z`, `rotation`, `scaleX`, `scaleY`) plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `speed` | `float` | `DEFAULT_POWER_UP_DRIFT_SPEED` (`-100` px/s) | Horizontal drift (negative: to the left). |

| Constant | Value | Description |
|----------|-------|-------------|
| `POWER_UP_RADIUS` | `12` px | Radius of the pickup area. |

## API

```cpp
void buildPowerUp(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);
```

```cpp
manager.registerPrefab(prefabs::POWER_UP_PREFAB, prefabs::buildPowerUp);
manager.spawn(prefabs::POWER_UP_PREFAB, buffer, {{"x", 800.0F}, {"y", 300.0F}, {"speed", -400.0F}});
```
