# Player Bullet

**Name:** `player_bullet` (`prefabs::PLAYER_BULLET_PREFAB`)
**Source:** `engine/src/prefabs/player_bullet/`

## Purpose

A fast projectile fired by a player, flying straight to the right towards the enemies.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | `x`, `y` from the spawn call |
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

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_PLAYER_BULLET_SPEED` | `900` px/s | Horizontal velocity (positive: to the right). |

## API

```cpp
void buildPlayerBullet(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed);
void registerPlayerBulletPrefab(PrefabManager& manager,
                                float speed = DEFAULT_PLAYER_BULLET_SPEED,
                                std::string_view name = PLAYER_BULLET_PREFAB);
```
