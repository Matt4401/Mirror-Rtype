# Alien Gunner

**Name:** `alien_gunner` (`prefabs::ALIEN_GUNNER_PREFAB`)
**Source:** `server/src/prefabs/alien_gunner/`

## Purpose

A slower enemy that fires at the ships. Like the [Alien Scout](alien-scout.md), it flies straight to the left and dies
on the first hit; its `prefabs::EnemyGun` makes it fire a missile at the nearest ship every `fire_interval` seconds.

The gun needs no script: `gameplay::EnemyFirePlugin` counts every `EnemyGun` down on each fixed step. A gun only fires
while its enemy is on the screen, and its first shot comes half an interval after it appears. Each shot is a
replicated `missile` aimed at the nearest ship that can be hit (not respawning), or straight ahead without any.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Kinematic`, `velocityX = speed` |
| `Collider2D` | Box, `halfWidth = halfHeight = ALIEN_GUNNER_HALF_SIZE` (`20` px), sensor, `ENEMY` / `ENEMY_MASK` |
| `prefabs::EnemyGun` | `interval = fire_interval`, `cooldown = interval / 2`, `shotSpeed = shot_speed` |

## Parameters

Every transform key read by `makeTransform` plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `speed` | `float` | `DEFAULT_ALIEN_GUNNER_SPEED` (`-90` px/s) | Horizontal velocity (negative: to the left). |
| `fire_interval` | `float` | `DEFAULT_ALIEN_GUNNER_FIRE_INTERVAL` (`1.8` s) | Seconds between two shots, clamped to 0.3–10 s. |
| `shot_speed` | `float` | `DEFAULT_ALIEN_GUNNER_SHOT_SPEED` (`300` px/s) | Speed of its missiles, clamped to 50–900 px/s. |

## Replication

Its missiles carry the shot look 1 as their `subtype` (`EnemyGun::shotLook`), so the clients can draw them their own way.

Kind `alien_gunner` (`rtype::game::KIND_ALIEN_GUNNER`).
