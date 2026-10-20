# Missile

**Name:** `missile` (`prefabs::MISSILE_PREFAB`)
**Source:** `engine/src/prefabs/missile/`

## Purpose

A fast projectile fired by enemies, flying straight to the left towards the player side.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | `x`, `y` from the spawn call |
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

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_MISSILE_SPEED` | `-600` px/s | Horizontal velocity (negative: to the left). |

## API

```cpp
void buildMissile(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed);
void registerMissilePrefab(PrefabManager& manager,
                           float speed = DEFAULT_MISSILE_SPEED,
                           std::string_view name = MISSILE_PREFAB);
```

Variants are registered under another name:

```cpp
prefabs::registerMissilePrefab(manager, -1200.0F, "fast_missile");
```
