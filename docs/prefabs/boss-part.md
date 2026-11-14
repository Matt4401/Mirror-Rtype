# Boss Part

**Name:** `boss_part` (`prefabs::BOSS_PART_PREFAB`)
**Source:** `server/src/prefabs/boss_part/`

## Purpose

A piece of a boss: a weak point, a turret or an armor plate. The parts are declared by the level `boss` event
(`[[events.parts]]`, see [Level Files](../gameplay/levels.md#boss-a-boss-fight)). `BossFightPlugin` spawns them next to
the boss and makes them its children in the entity hierarchy: they move with the boss and are destroyed with it.

## Roles

| Role | Name in level files | A player shot on it |
|------|---------------------|---------------------|
| `BossPartRole::WeakPoint` | `weak_point` | Removes one hit point from the boss. A boss with a weak point only takes damage through it: shots on its body stop without damage. |
| `BossPartRole::Turret` | `turret` | Removes one of its own hit points; at 0 the turret alone is destroyed and the shooter scores. |
| `BossPartRole::Armor` | `armor` | Stops without any damage. |

## Components

| Component | Values |
|-----------|--------|
| `Transform` | built from the spawn parameters (`makeTransform`) |
| `Renderable` | `layer = 2` |
| `RigidBody2D` | `Kinematic`, no velocity: the hierarchy moves it |
| `Collider2D` | Box of `width` × `height`, sensor, `ENEMY` / `ENEMY_MASK` |
| `prefabs::BossPart` | `role`, `look` |
| `prefabs::Health` | turrets only: `points = max = health` |

## Parameters

Every transform key read by `makeTransform` plus:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `role` | string | `armor` | `weak_point`, `turret` or `armor`; any other value gives an armor. `BossFightPlugin` skips a level part with an unknown role instead. |
| `width`, `height` | number | `DEFAULT_BOSS_PART_SIZE` (`48` px) | Hitbox size, clamped to 8–1024 px. |
| `health` | number | `DEFAULT_TURRET_HEALTH` (`8`) | Hit points of a turret, clamped to 1–255. |
| `look` | number | `0` | Look of the part, clamped to 0–255 and sent as its `subtype`. |

## Replication

Kind `boss_part` (`rtype::game::KIND_BOSS_PART`): `subtype` is the look, `health` the hit points left of a turret (1 for
the other parts).

## API

```cpp
void buildBossPart(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);
std::optional<BossPartRole> bossPartRole(std::string_view name);
```
