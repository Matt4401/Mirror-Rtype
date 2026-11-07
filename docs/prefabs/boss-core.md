# Boss Core

**Name:** `boss_core` (`prefabs::BOSS_CORE_PREFAB`)
**Source:** `server/src/prefabs/boss_core/`

## Purpose

A large armored enemy that ends a level. It is spawned by a level `boss` event, which stops the scroll until the boss is
destroyed. Unlike the other enemies, it survives many hits: its hit points are a `prefabs::Health` component.

Its moves and attacks come from the Wren script named by the `boss` event (`script = "scripts/Boss.wren"`). Without a
script, it flies in from the right and stops at `stop_x`.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Kinematic`, `velocityX = speed` |
| `Collider2D` | Box, `halfWidth = BOSS_CORE_HALF_WIDTH`, `halfHeight = BOSS_CORE_HALF_HEIGHT`, sensor |
| `prefabs::Health` | `points = max = health` |
| `prefabs::BossCore` | `stopX = stop_x` |

## Collision

| Category | Mask |
|----------|------|
| `ENEMY` | `ENEMY_MASK` = `PLAYER \| PLAYER_SHOT` |

Same as the [Alien Scout](alien-scout.md): the boss hits the player and is hit by player bullets. See
[Collision Categories](collision-categories.md).

## Parameters

Every transform key read by `makeTransform` (`x`, `y`, `z`, `rotation`, `scaleX`, `scaleY`) plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `speed` | `float` | `DEFAULT_BOSS_CORE_SPEED` (`-120` px/s) | Horizontal velocity while flying in (negative: to the left). |
| `stop_x` | `float` | `DEFAULT_BOSS_CORE_STOP_X` (`1000` px) | Abscissa at which the boss stops flying in. |
| `health` | number | `DEFAULT_BOSS_CORE_HEALTH` (`40`) | Hit points, clamped to 1–255 so that they fit the network `health` field. |

| Constant | Value | Description |
|----------|-------|-------------|
| `BOSS_CORE_HALF_WIDTH` | `64` px | Half-width of the hitbox. |
| `BOSS_CORE_HALF_HEIGHT` | `80` px | Half-height of the hitbox. |

## API

```cpp
void buildBossCore(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);
```

```cpp
manager.registerPrefab(prefabs::BOSS_CORE_PREFAB, prefabs::buildBossCore);
manager.spawn(prefabs::BOSS_CORE_PREFAB, buffer, {{"x", 1328.0F}, {"y", 360.0F}, {"health", 60}});
```
