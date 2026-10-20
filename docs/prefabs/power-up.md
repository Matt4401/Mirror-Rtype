# Power-Up

**Name:** `power_up` (`prefabs::POWER_UP_PREFAB`)
**Source:** `server/src/prefabs/power_up/`

## Purpose

A collectible bonus slowly drifting to the left with the scrolling. Its collider is a sensor: it detects the player without physically blocking it.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | `x`, `y` from the spawn call |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Kinematic`, `velocityX = driftSpeed` |
| `Collider2D` | Circle, `radius = POWER_UP_RADIUS`, sensor |

## Collision

| Category | Mask |
|----------|------|
| `POWER_UP` | `POWER_UP_MASK` = `PLAYER` |

Only players can pick it up. Shots, enemies and obstacles go through it. See [Collision Categories](collision-categories.md).

## Parameters

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_POWER_UP_DRIFT_SPEED` | `-100` px/s | Horizontal velocity (negative: to the left). |
| `POWER_UP_RADIUS` | `12` px | Radius of the pickup area. |

## API

```cpp
void buildPowerUp(ecs::Entity e, ecs::Registry& registry, float x, float y, float driftSpeed);
void registerPowerUpPrefab(PrefabManager& manager,
                           float driftSpeed = DEFAULT_POWER_UP_DRIFT_SPEED,
                           std::string_view name = POWER_UP_PREFAB);
```
