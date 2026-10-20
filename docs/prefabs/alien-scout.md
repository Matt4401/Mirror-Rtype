# Alien Scout

**Name:** `alien_scout` (`prefabs::ALIEN_SCOUT_PREFAB`)
**Source:** `engine/src/prefabs/alien_scout/`

## Purpose

A basic enemy ship flying straight to the left at constant speed.

!!! note
    The scout has no AI yet. Once enemy scripts exist, attach a `ScriptComponent` in `buildAlienScout`.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | `x`, `y` from the spawn call |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Kinematic`, `velocityX = speed` |
| `Collider2D` | Box, `halfWidth = halfHeight = ALIEN_SCOUT_HALF_SIZE`, sensor |

## Collision

| Category | Mask |
|----------|------|
| `ENEMY` | `ENEMY_MASK` = `PLAYER \| PLAYER_SHOT` |

The scout hits the player and is hit by player bullets. It ignores other enemies, enemy shots, obstacles and power-ups, so it flies through level geometry. See [Collision Categories](collision-categories.md).

## Parameters

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_ALIEN_SCOUT_SPEED` | `-150` px/s | Horizontal velocity (negative: to the left). |
| `ALIEN_SCOUT_HALF_SIZE` | `16` px | Half-size of the square hitbox. |

## API

```cpp
void buildAlienScout(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed);
void registerAlienScoutPrefab(PrefabManager& manager,
                              float speed = DEFAULT_ALIEN_SCOUT_SPEED,
                              std::string_view name = ALIEN_SCOUT_PREFAB);
```
