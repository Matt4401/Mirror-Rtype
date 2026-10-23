# Missile

**Name:** `missile` (`prefabs::MISSILE_PREFAB`)
**Source:** `server/src/prefabs/missile/`

## Purpose

A fast projectile fired by enemies, flying straight to the left towards the player side.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Dynamic`, `velocityX = speed`, `bullet = true` |
| `Collider2D` | Box, `halfWidth = 8`, `halfHeight = 3`, sensor |

## Collision

| Category | Mask |
|----------|------|
| `ENEMY_SHOT` | `ENEMY_SHOT_MASK` = `PLAYER \| OBSTACLE` |

The missile hits the player and is stopped by obstacles. It ignores enemies, other shots and power-ups. See [Collision Categories](collision-categories.md).

!!! note "Why `Dynamic`"
    Box2D only creates a contact when at least one of the two bodies is `Dynamic`. The missile must detect `Kinematic` or `Static` bodies, so it is `Dynamic`. The world has zero gravity and the collider is a sensor: the body keeps its velocity and never pushes anything, like a `Kinematic` body.

## Parameters

Every transform key read by `makeTransform` (`x`, `y`, `z`, `rotation`, `scaleX`, `scaleY`) plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `speed` | `float` | `DEFAULT_MISSILE_SPEED` (`-600` px/s) | Horizontal velocity (negative: to the left). |

## API

```cpp
void buildMissile(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);
```

```cpp
manager.registerPrefab(prefabs::MISSILE_PREFAB, prefabs::buildMissile);
manager.spawn(prefabs::MISSILE_PREFAB, buffer, {{"x", 800.0F}, {"y", 300.0F}, {"speed", -400.0F}});
```

Variants are registered under another name with their own defaults:

```cpp
manager.registerPrefab("fast_missile", prefabs::buildMissile, {{"speed", -1200.0F}});
```
