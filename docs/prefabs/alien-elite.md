# Alien Elite

**Name:** `alien_elite` (`prefabs::ALIEN_ELITE_PREFAB`)
**Source:** `server/src/prefabs/alien_elite/`

## Purpose

A rare, tough enemy, meant to appear a few times per level. It flies slowly to the left while weaving up and down, fires
fans of three shots at the nearest ship, takes several hits, and drops a `power_up` where a player kills it.

No script is involved:

- `gameplay::EnemyMotionPlugin` moves it along its `prefabs::SineWave`, around the height it appeared at;
- `gameplay::EnemyFirePlugin` fires its `prefabs::EnemyGun` (`GunPattern::Spread`: one shot at the nearest ship and one
  `SPREAD_ANGLE` on each side) while it is on the screen;
- `gameplay::CombatPlugin` removes one of its hit points per shot, and spawns the power-up of its `prefabs::DropsPowerUp`
  when it dies.

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 1` |
| `RigidBody2D` | `Kinematic`, `velocityX = speed` |
| `Collider2D` | Box, `halfWidth = halfHeight = ALIEN_ELITE_HALF_SIZE` (`28` px), sensor, `ENEMY` / `ENEMY_MASK` |
| `prefabs::Health` | `points = max = health` |
| `prefabs::SineWave` | `amplitude`, `frequency`, centered on the spawn height |
| `prefabs::EnemyGun` | `interval = fire_interval`, `cooldown = interval / 2`, `shotSpeed = shot_speed`, `Spread` |
| `prefabs::DropsPowerUp` | |

## Parameters

Every transform key read by `makeTransform` plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `speed` | `float` | `-70` px/s | Horizontal velocity (negative: to the left). |
| `health` | number | `6` | Hit points, clamped to 1–255. |
| `amplitude` | number | `90` px | Half-height of the weave, clamped to 0–360 px. |
| `frequency` | number | `0.35` | Weaves per second, clamped to 0–4. |
| `fire_interval` | number | `2.4` s | Seconds between two fans, clamped to 0.3–10 s. |
| `shot_speed` | number | `260` px/s | Speed of its missiles, clamped to 50–900 px/s. |

## Replication

Kind `alien_elite` (`rtype::game::KIND_ALIEN_ELITE`): `health` is its hit points left. The clients flash it on each hit
and show a burst when it is killed.
