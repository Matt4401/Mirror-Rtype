# Timeline

## Purpose

A timeline is a list of events ordered by a position, played by a cursor that moves forward. When the cursor reaches an
event, the event fires once. The engine gives no meaning to the position or to the events: a game decides what the
cursor measures (scroll distance, seconds, beats...) and what each event type does.

R-Type uses it for its levels: the cursor is the scroll distance, so events wait while the scroll is stopped during a
boss fight. See [Level Files](../gameplay/levels.md) for the R-Type format built on it.

The module is `engine_timeline` (`engine/src/timeline/`). It is headless and depends only on `engine_core` and
`engine_events`: it does not know about prefabs.

## `timeline::TimelineEvent`

| Field | Type | Description |
|-------|------|-------------|
| `position` | `float` | Cursor position at which the event fires. |
| `type` | `std::string` | Event type, e.g. `"spawn"`; interpreted by whoever handles the event. |
| `params` | `ecs::ParamBag` | Event data. `prefabs::PrefabParams` is the same type, so a spawn can hand it to a prefab as is. |

## `timeline::Timeline`

A pure data structure: no clock, no engine.

| Method | Description |
|--------|-------------|
| `add(event)` | Inserts an event, keeping the events sorted by position. Events at the same position keep their insertion order. An event added behind the cursor fires on the next move. |
| `addAll(events)` | Same as `add` for each event, with a single sort: use it to build a large timeline. |
| `advanceTo(position)` / `advance(delta)` | Moves the cursor forward and returns the events reached, in order. A position behind the cursor leaves it in place. |
| `rewind()` | Puts the cursor back to 0: every event can fire again. |
| `clear()` | Removes every event and puts the cursor back to 0. |
| `cursor()`, `events()`, `pending()`, `finished()` | Inspection. |

A large step fires every event it passes over, in order: nothing is skipped and nothing fires twice, whatever the frame
rate. The cursor is a `double`, so small steps still add up far along a long timeline.

Copies share their events: copying a loaded level into every game costs a pointer, and each copy keeps its own cursor.
A copy that changes its events gets its own list first (copy-on-write).

## `timeline::TimelinePlugin`

Plays a timeline: every fixed step, it moves the cursor by `speed * dt` and publishes on the `EventBus`:

| Event | When |
|-------|------|
| `TimelineEventFired{event}` | Once per event reached, in firing order. It holds a copy, so a handler may change the timeline (add events, rewind, replace it). |
| `TimelineFinished{}` | Once per play-through, on the step the last pending event fires. Events added after the timeline finished fire without a new `TimelineFinished`, until it is rewound or replaced. |

| Method | Description |
|--------|-------------|
| `timeline()` | The timeline being played, to fill or replace. |
| `setSpeed(unitsPerSecond)` / `speed()` | Cursor speed; 0 pauses playback. Negative or non-finite values are ignored. |
| `step(engine, delta)` | Moves the cursor right now, e.g. to start at a checkpoint or in tests. |

```cpp
engine.addPlugin<timeline::TimelinePlugin>();
engine.init();

auto& player = engine.getPlugin<timeline::TimelinePlugin>();
player.timeline().add({.position = 400.0F, .type = "spawn", .params = {{"prefab", std::string("alien_scout")}}});
player.setSpeed(60.0F);  // 60 units per second

engine.eventBus().subscribe<timeline::TimelineEventFired>([](const timeline::TimelineEventFired& fired) {
    // The game gives a meaning to fired.event.type.
});
```

## TOML arrays in `config::ConfigFile`

Level files are TOML lists of events, so `engine_config` reads arrays too. Each element is flattened into an indexed
key, and `arraySize(key)` gives the element count. Existing files without arrays read exactly as before.

```toml
keys = [10, 20]          # keys.0 = 10, keys.1 = 20, arraySize("keys") = 2

[[events]]
at = 400.0               # events.0.at
type = "spawn"           # events.0.type

[[events]]
at = 900.0               # events.1.at, arraySize("events") = 2
```

`keysUnder("events.0")` lists the keys of one element, which lets a game pass the keys it does not know to a prefab.
