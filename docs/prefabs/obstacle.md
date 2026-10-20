# Obstacle

**Name:** `obstacle` (`prefabs::OBSTACLE_PREFAB`)
**Source:** `engine/src/prefabs/obstacle/`

## Purpose

A static solid block of level geometry. Unlike the other prefabs, its size is a parameter of the prefab rather than its speed.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | `x`, `y` from the spawn call (center of the block) |
| `Renderable` | default (`layer = 0`) |
| `RigidBody2D` | `Static` |
| `Collider2D` | Box, `halfWidth`, `halfHeight` from the registration, **solid** (not a sensor) |

## Collision

| Category | Mask |
|----------|------|
| `OBSTACLE` | `OBSTACLE_MASK` = `ALL` |

The obstacle accepts every category. Whether a pair actually interacts is decided by the other collider's mask: players and both kinds of shots touch it, enemies and power-ups pass through it. See [Collision Categories](collision-categories.md).

## Parameters

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_OBSTACLE_HALF_WIDTH` | `32` px | Half-width of the collision box. |
| `DEFAULT_OBSTACLE_HALF_HEIGHT` | `32` px | Half-height of the collision box. |

## API

```cpp
void buildObstacle(ecs::Entity e, ecs::Registry& registry, float x, float y,
                   float halfWidth, float halfHeight);
void registerObstaclePrefab(PrefabManager& manager,
                            float halfWidth = DEFAULT_OBSTACLE_HALF_WIDTH,
                            float halfHeight = DEFAULT_OBSTACLE_HALF_HEIGHT,
                            std::string_view name = OBSTACLE_PREFAB);
```

Size variants are registered under another name:

```cpp
prefabs::registerObstaclePrefab(manager, 128.0F, 16.0F, "obstacle_wide");
```
