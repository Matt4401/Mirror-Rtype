# Obstacle

**Name:** `obstacle` (`prefabs::OBSTACLE_PREFAB`)
**Source:** `server/src/prefabs/obstacle/`

## Purpose

A static solid block of level geometry. Unlike the other prefabs, its size is a parameter of the prefab rather than its speed.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`), `x`, `y` being the center of the block |
| `Renderable` | default (`layer = 0`) |
| `RigidBody2D` | `Static` |
| `Collider2D` | Box, `halfWidth`, `halfHeight` from the spawn parameters, **solid** (not a sensor) |

## Collision

| Category | Mask |
|----------|------|
| `OBSTACLE` | `OBSTACLE_MASK` = `ALL` |

The obstacle accepts every category. Whether a pair actually interacts is decided by the other collider's mask: players and both kinds of shots touch it, enemies and power-ups pass through it. See [Collision Categories](collision-categories.md).

## Parameters

Every transform key read by `makeTransform` (`x`, `y`, `z`, `rotation`, `scaleX`, `scaleY`) plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `halfWidth` | `float` | `DEFAULT_OBSTACLE_HALF_WIDTH` (`32` px) | Half-width of the collision box. |
| `halfHeight` | `float` | `DEFAULT_OBSTACLE_HALF_HEIGHT` (`32` px) | Half-height of the collision box. |

## API

```cpp
void buildObstacle(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);
```

```cpp
manager.registerPrefab(prefabs::OBSTACLE_PREFAB, prefabs::buildObstacle);
manager.spawn(prefabs::OBSTACLE_PREFAB, buffer, {{"x", 800.0F}, {"y", 300.0F}, {"halfWidth", 64.0F}, {"halfHeight", 64.0F}});
```

Variants are registered under another name with their own defaults:

```cpp
manager.registerPrefab("obstacle_wide", prefabs::buildObstacle, {{"halfWidth", 128.0F}, {"halfHeight", 16.0F}});
```
