# Bosses

The bosses of the game are original designs. Each one is a level `boss` event (see [Level Files](levels.md#boss-a-boss-fight)):
a `boss_core` with its `variant` (its look on the clients, see the [boss looks file](../architecture/client.md#boss-looks-file-clientconfigbossestoml)),
its [parts](../prefabs/boss-part.md) and a Wren script under `scripts/bosses/` that moves it and attacks with the
`game/Combat` helpers.

## Hive Warden (`scripts/bosses/Warden.wren`, variant 1)

A drifting carrier guarded by turrets. Its body is armored: only its weak point hurts it.

| Phase | Behavior |
|-------|----------|
| Entrance (3 s) | Flies in to its `stop_x`, does not attack. |
| Cycle | Drifts along a slow figure of eight. Releases a fan of four drones (`alien_scout`), then each living turret fires a burst of three shots aimed at the nearest ship. |
| Frenzy | Once every turret is down, or under half of its hit points: the core fires a three-shot spread at the nearest ship, the drift and the cycle speed up. |
| Death | Drops a power-up. Its parts are destroyed with it. |

Suggested event:

```toml
[[events]]
at = 5400.0
type = "boss"
prefab = "boss_core"
variant = 1
health = 30
width = 160.0
height = 200.0
script = "scripts/bosses/Warden.wren"

[[events.parts]]
role = "weak_point"
x = -60.0
width = 40.0
height = 40.0

[[events.parts]]
role = "turret"
y = -100.0
health = 6
look = 1

[[events.parts]]
role = "turret"
y = 100.0
health = 6
look = 1
```

Destroying the turrets first takes away the aimed bursts but triggers the frenzy: a trade-off for the players.
