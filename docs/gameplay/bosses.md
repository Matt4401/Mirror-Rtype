# Bosses

The bosses of the game are original designs. Each one is a level `boss` event (see [Level Files](levels.md#boss-a-boss-fight)):
a `boss_core` with its `variant` (its look on the clients, see the [looks file](../architecture/client.md#looks-file-clientconfiglookstoml)),
its [parts](../prefabs/boss-part.md) and a Wren script under `scripts/bosses/` that moves it and attacks with the
`game/Combat` helpers.

The Bastion ends the last level of the shipped campaign (`server/levels/level_03.toml`); the Hive Warden is ready for a
level that wants it.

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

## Bastion (`scripts/bosses/Bastion.wren`, variant 2)

An armored fortress whose weak point hides behind two armor plates. It changes tactics as it loses its hit points:
each phase is checked at the end of a round. All its shots carry the red look of the turret shots (`shotLook`, 1).

| Phase | Hit points left | Behavior |
|-------|-----------------|----------|
| Entrance (3 s) | | Flies in to its `stop_x`, does not attack. |
| 1. Siege | above two thirds | Slides up and down. Each turret fires a straight shot to the left, then the core fires one shot aimed at the nearest ship. |
| 2. Assault | above one third | Charges 420 px towards the ships and back, dropping slow mines (missiles drifting left at 60 px/s) on the way, then fires an aimed shot. |
| 3. Last stand | below one third | Goes back to its place and fires a spiral: ten shots all around it per turn, the spiral turning at each turn. |
| Death | | Drops two power-ups. |

Suggested event:

```toml
[[events]]
at = 6300.0
type = "boss"
prefab = "boss_core"
variant = 2
health = 45
width = 280.0
height = 90.0
script = "scripts/bosses/Bastion.wren"

[[events.parts]]
role = "weak_point"
x = -30.0
width = 36.0
height = 36.0

[[events.parts]]
role = "armor"
x = -75.0
y = -24.0
width = 22.0
height = 32.0

[[events.parts]]
role = "armor"
x = -75.0
y = 24.0
width = 22.0
height = 32.0

[[events.parts]]
role = "turret"
x = -110.0
y = -36.0
width = 32.0
height = 32.0
health = 8
look = 2

[[events.parts]]
role = "turret"
x = 100.0
y = -36.0
width = 32.0
height = 32.0
health = 8
look = 2
```

The hitbox is a long, flat warship (280 x 90). The armor plates sit in front of the weak point and leave a narrow
slit between them: shots go through it, or come from above or below, which the slide of the siege phase and the
charges of the assault phase make harder. The turrets sit on top of the hull, at the bow and at the stern.
