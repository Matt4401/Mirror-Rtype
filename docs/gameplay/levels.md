# Level Files

## Purpose

An R-Type level is a TOML file: enemy waves, scroll speed changes, terrain, a boss and the end of the level. Level
designers create and tune stages without touching C++ code or recompiling.

Levels are read by the authoritative server only (`server/levels/`). The clients receive what a level spawns through
replication, like any other entity. The server loads `server/levels/level_01.toml` at start-up, relative to the
directory it runs from (the project root), and every room plays it.

The engine only provides generic building blocks, the TOML arrays of `config::ConfigFile` and the
[Timeline](../architecture/timeline.md). The format below and its loader (`server/src/level/`, target `rtype_level`)
belong to R-Type.

## Positions are scroll distances

Every position (`at`, `from`, `to`) is a **scroll distance in pixels**, not a time. The timeline cursor moves at the
current scroll speed: at 60 px/s, `at = 600.0` fires after 10 s. When the scroll stops (during a boss fight), the
level waits.

## Example

```toml
[level]
name = "Sector 1"         # level name (logs)
scroll_speed = 60.0       # px/s at the start

[[events]]                # each [[events]] block adds one event to the list
at = 400.0
type = "spawn"
prefab = "alien_scout"
count = 5                 # a wave of 5 enemies...
interval = 0.4            # ...one every 0.4 s
y = 200.0
spacing_y = 40.0          # each next enemy 40 px lower
pattern = "sine"          # unknown keys are passed to the prefab

[[events]]
at = 2400.0
type = "scroll"
speed = 30.0              # slows down

[[events]]
at = 6000.0
type = "boss"
prefab = "boss_core"
script = "scripts/Boss.wren"

[[events]]
at = 6600.0
type = "end"

[[terrain]]
from = 1800.0
to = 3200.0
side = "bottom"
height = 120.0
```

## `[level]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `name` | string | `"Unnamed level"` | Display name, in the server logs. |
| `scroll_speed` | number | `60` | Scroll speed at the start, in px/s, from 10 to 2000. |

## `[[events]]`

Every event has an `at` (scroll distance, from 0 to 1 000 000 px) and a `type`. The events are sorted by `at`; events
with the same `at` keep the file order.

### `spawn`: a wave of enemies

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `prefab` | string | required | Enemy to spawn. The server knows `alien_scout` and `boss_core`. |
| `count` | integer | `1` | Number of enemies, from 1 to 32. |
| `interval` | number | `0` | Seconds of scrolling between two enemies, from 0 to 10. |
| `x` | number | `1328` | Spawn abscissa: just past the right edge of the 1280 px screen. |
| `y` | number | `360` | Spawn ordinate of the first enemy. |
| `spacing_y` | number | `0` | Added to `y` for each next enemy. |
| any other key | scalar | | Passed to the prefab as is (e.g. `speed`, `pattern`). |

A wave is expanded at load time into one spawn per enemy. The enemies are spaced by `interval` seconds of scrolling,
following the `scroll` events on the way: if the scroll slows down in the middle of a wave, its next enemies come closer
in distance but still `interval` seconds apart. While the scroll is stopped (boss fight, respawn), the rest of the wave
waits too.

### `scroll`: change the scroll speed

| Key | Type | Description |
|-----|------|-------------|
| `speed` | number | New scroll speed in px/s, from 10 to 2000. |

The speed cannot be 0: positions are scroll distances, so a stopped scroll would never reach the next event and the
level would be frozen for good. Only the game stops the scroll, during a boss fight or a respawn, and it always resumes
it.

### `boss`: a boss fight

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `prefab` | string | required | Boss prefab: `boss_core`. |
| `script` | string | none | Wren file driving the boss: a relative path ending in `.wren`, with `/` separators and no `..`. |
| `class` | string | file name | Wren class of the script; `scripts/Boss.wren` uses the class `Boss`. |
| `x`, `y` | number | `1328`, `360` | Spawn position. |
| `health`, `stop_x`, `speed` | number | see [Boss Core](../prefabs/boss-core.md) | Passed to the prefab, like any other key. |

When the event fires, the scroll stops and the boss flies in from the right until its `stop_x`. Each player shot
removes one hit point; at 0, the boss is destroyed, the shooter scores, and the scroll resumes at its speed before the
fight. The script moves the boss and attacks: everything it spawns with `Engine.spawnPrefab` is replicated. Its
missiles and the boss itself destroy the ships they touch (they respawn), and the power-up it drops gives rapid fire to
the player who picks it up. See `scripts/Boss.wren`.

Events at the same distance as the boss (or reached in the same server step) wait for the end of the fight, then play
in order: a `scroll` there does not restart the scroll, and an `end` does not restart the level while the boss is alive.
Another `boss` at the same distance joins the current fight.

### `end`: end of the level

No key. For now there is a single level, so `end` restarts it from the start.

## `[[terrain]]`

A solid ground or ceiling that scrolls with the level. Touching it destroys a ship; player shots stop on it.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `from` | number | required | Scroll distance at which the section enters from the right edge. |
| `to` | number | required | Scroll distance at which it ends; must be greater than `from`. |
| `side` | string | required | `"top"` or `"bottom"`. |
| `height` | number | `120` | Height in pixels, from 1 to 360 (half of the screen, so a passage always stays open). |

The server builds a section as **blocks of 256 px** (`obstacle` entities, `rtype::game::TERRAIN_BLOCK_WIDTH`) as they
enter the screen, so a section on the screen is about 6 entities per side in the snapshots. A block carries its height
in its `subtype` (2 px units, see the [protocol](../protocol/packets.md#appendix-a--r-type-entity-conventions)). The
length is rounded up to whole blocks, and the overlap checks below use this rounded end: write `from` and `to` as
multiples of 256 to get exactly what the file says.
Terrain moves at the scroll speed: it stops during a boss fight too. A ship touching it is destroyed and respawns at
its start position, blinking and invulnerable for 3 seconds, while the scroll waits; it can fly out of the terrain in
the meantime.

## Validation

Level files are server data, but they are validated like untrusted input: a bad file never crashes the server and
never spawns without bounds.

| Problem | What happens |
|---------|--------------|
| Missing file, or not valid TOML | The server prints the error and does not start. |
| Out-of-range number | Clamped to its bounds, with a warning. |
| Missing `at` or `type`, unknown `type` | The event is skipped, with a warning. |
| Spawn or boss without `prefab`, or with an unknown one | The event is skipped, with a warning. |
| Unsafe `script` path | The script is ignored, the boss is kept, with a warning. |
| Invalid terrain section | The section is skipped, with a warning. |
| Unknown key in `[level]` or in a terrain section | A warning, usually a typo. |
| More than 1024 events or 256 terrain sections | The extra ones are ignored, with a warning. |

Warnings are printed at start-up, prefixed by the key they are about (`level: events.3: unknown prefab 'dragon',
event skipped`). The shipped levels are checked by the `ShippedLevels` tests: they must load without any warning.

## Adding an event type

Later event types (`checkpoint`, a campaign of several levels...) fit the format without breaking it:

1. Add its type name in `server/src/level/Level.hpp`.
2. Read and validate its keys in `buildLevel()` (`server/src/level/LevelLoader.cpp`), and turn it into timeline events.
3. Handle its `LevelEventPlayed` in a gameplay plugin (`server/src/gameplay/`), or in `LevelDirectorPlugin::play()`.
4. Cover it in `tests/server/LevelLoaderTests.cpp` and document it here.
