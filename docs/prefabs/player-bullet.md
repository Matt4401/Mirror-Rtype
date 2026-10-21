# Player Bullet

**Name:** `player_bullet` (`prefabs::PLAYER_BULLET_PREFAB`)
**Source:** `server/src/prefabs/player_bullet/`

## Purpose

A fast projectile fired by a player, flying straight to the right towards the enemies.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Dynamic`, `velocityX = speed`, `bullet = true` |
| `Collider2D` | Box, `halfWidth = 6`, `halfHeight = 2`, sensor |

## Collision

| Category | Mask |
|----------|------|
| `PLAYER_SHOT` | `PLAYER_SHOT_MASK` = `ENEMY \| OBSTACLE` |

The bullet hits enemies and is stopped by obstacles. It never hits a player (no friendly fire), other shots or power-ups. See [Collision Categories](collision-categories.md).

!!! note "Why `Dynamic`"
    Box2D only creates a contact when at least one of the two bodies is `Dynamic`. The bullet must detect `Kinematic` or `Static` bodies, so it is `Dynamic`. The world has zero gravity and the collider is a sensor: the body keeps its velocity and never pushes anything, like a `Kinematic` body.

## Parameters

Every transform key read by `makeTransform` (`x`, `y`, `z`, `rotation`, `scaleX`, `scaleY`) plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `speed` | `float` | `DEFAULT_PLAYER_BULLET_SPEED` (`900` px/s) | Horizontal velocity (positive: to the right). |

## API

```cpp
void buildPlayerBullet(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);
```

```cpp
manager.registerPrefab(prefabs::PLAYER_BULLET_PREFAB, prefabs::buildPlayerBullet);
manager.spawn(prefabs::PLAYER_BULLET_PREFAB, buffer, {{"x", 800.0F}, {"y", 300.0F}, {"speed", -400.0F}});
```
