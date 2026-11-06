# RLSS

RLSS is the second sample game built on the engine (Track #1): a 2D, side-view car football game whose car physics
are modelled on Rocket League. It proves that the engine is reusable: it lives outside `engine/`, links only the
engine modules it needs, and the engine knows nothing about it (the game does not modify `engine/` at all).

## Layout

```
rlss/
├── xmake.lua           -- includes the targets below (the client only with --graphics=y)
├── shared/             -- rlss_shared: protocol + codec, arena dimensions, player palette (no engine)
├── game/               -- rlss_game: game components, replication codec, Wren bindings (server + client)
├── server/             -- rlss_server: headless, authoritative
├── client/             -- rlss_client: window, arena and car drawing, controls
└── scripts/            -- Wren: Arena, CarTuning, Car (physics, server), CarView (car look, client)
```

| Target        | Engine modules used                                                                               |
|---------------|---------------------------------------------------------------------------------------------------|
| `rlss_shared` | none                                                                                              |
| `rlss_game`   | `engine_core`, `engine_network`, `engine_input`, `engine_scripting`                               |
| `rlss_server` | `rlss_game` + `engine_time`, `engine_events`, `engine_network_asio`                               |
| `rlss_client` | `rlss_game` + `engine_time`, `engine_events`, `engine_render`, `engine_network_asio`             |

## Running

```sh
xmake
./rlss_server [port]            # default port 4250
./rlss_client [host] [port]     # default 127.0.0.1 4250, start as many as you want
```

Run both from the repository root: the Wren scripts are loaded from `rlss/scripts/`. Scripts are hot-reloaded: save
`rlss/scripts/CarTuning.wren` while the server runs to tune the physics live.

| Action                             | Keyboard                    | Gamepad           |
|------------------------------------|-----------------------------|-------------------|
| Drive (ground) / rotate (air)      | A / D, Q / D, ← / →         | left stick        |
| Jump, then double jump or dodge    | Space, W, Z, ↑              | bottom face button |

## How it plugs into the engine

Both executables are an `engine::Engine` plus plugins; game plugins never call each other, they communicate through
the `EventBus` and the ECS.

```mermaid
flowchart LR
    subgraph Client
        IP[InputPlugin] --> PIP[PlayerInputPlugin] -- LocalInput --> CRP[ClientReplicationPlugin]
        CRP -- snapshot applied, interpolated --> CV[CarView.wren]
    end
    subgraph Server
        SRP[ServerReplicationPlugin] -- InputApplied --> CSP[CarSimulationPlugin]
        CSP -- InputState --> CAR[Car.wren]
        CAR -- Transform --> SRP
    end
    CRP -- PlayerInput, UDP --> SRP
    SRP -- Snapshot, UDP --> CRP
```

- **Server** (`rlss/server/main.cpp`): `NetworkPlugin` fed by `ServerConnection` (game-side adapter, one engine owns
  every peer), `ServerReplicationPlugin` with the RLSS `ReplicationCodec`, `ScriptPlugin`, then:
    - `CarSimulationPlugin` exposes the components to Wren, spawns the arena entity (tag `arena` + `ArenaShape`) and
      copies each applied input into the car's `InputState`;
    - `LobbyPlugin` gives each peer a slot (color + spawn point) and a car: `Transform`, `InputState`, `InputBuffer`,
      `NetworkOwner`, `CarLook`, `ScriptComponent{Car}`, replicated with kind `KIND_CAR`. It also broadcasts the
      `LobbyState` when a player joins or leaves.
- **Client** (`rlss/client/main.cpp`): connects first (no window if the server is unreachable), opens UDP, then
  `RenderPlugin`, `InputPlugin`, `NetworkPlugin` fed by `ServerLink`, `ClientReplicationPlugin`, `ScriptPlugin`, and:
    - `ArenaViewPlugin` draws the arena with flat colored rectangles and frames it with the camera;
    - `CarViewPlugin` gives each replicated car its parts (body and cabin in the player color, wheels, headlight) and
      the `CarView` script that places them every frame;
    - `PlayerInputPlugin` publishes the local controls as a `LocalInput` every fixed step;
    - `SessionPlugin` logs the player count and closes the game when the connection is lost.

### What is scripted

| Wren module              | Runs on | Role                                                                     |
|--------------------------|---------|--------------------------------------------------------------------------|
| `rlss/scripts/Arena`     | server  | Arena shape as a signed distance field: distance, wall normal, curvature |
| `rlss/scripts/CarTuning` | server  | Rocket League constants                                                   |
| `rlss/scripts/Car`       | server  | Whole car physics, one instance per car                                   |
| `rlss/scripts/CarView`   | client  | Size and placement of the car parts, wheel spin                           |

The scripts see the ECS through component classes bound by `rlss::game::installScriptBindings` in the
`rlss/components` module: `Transform`, `Controls` (`InputState.playerIndex`), `ArenaShape`, `CarParts`. They read the
player input with the engine's `Input.getAxis(player, "steer")` / `Input.isDown(player, "jump")`.

What stays in C++: creating entities (Wren can only spawn prefabs at a position), the network and replication, and
the arena drawing (hundreds of shapes created once).

## Car physics

Side view, y down, Rocket League units (uu). The arena is 1920 × 1024 uu with 280 uu curved corners; the car hitbox is
118 × 36 uu. `Car.wren` steps at a fixed 120 Hz whatever the frame rate.

| Quantity                          | Value                                    |
|-----------------------------------|------------------------------------------|
| Gravity                           | 650 uu/s²                                |
| Throttle acceleration             | 1600 uu/s² at rest → 160 at 1400 → 0 at 1410 uu/s |
| Braking / coasting                | 3500 / 525 uu/s²                         |
| Sticky force on any surface       | 325 uu/s²                                |
| Jump                              | 292 uu/s + 1458 uu/s² while held, 0.2 s max |
| Second jump window                | 1.25 s after the first jump              |
| Dodge                             | 500 uu/s forward (533 back), scaled by speed; spins at 5.5 rad/s for 0.65 s |
| Air control                       | 12.46 rad/s², damping 2.798, max 5.5 rad/s |
| Max speed                         | 2300 uu/s                                |

- **On the wheels**, the car follows the surface: floor, curved corners, walls, ceiling. Gravity along the surface
  slows a climb. The surface holds the car while `−g·n + v²·curvature + sticky ≥ 0`: always on the floor and walls,
  on the curved corners only if fast enough, never on the flat ceiling. There the suspension (20 uu) extends for a
  moment before the wheels lose contact, so the car drives on the ceiling only briefly.
- The stick is screen-relative but locked while held: holding "right" drives up the right wall and along the ceiling.
  Steering against the motion brakes; below 200 uu/s it turns the car around (Sideswipe-style mirror).
- **In the air**: jump hold, double jump (no direction) or dodge (toward the stick), air rotation. The four hitbox
  corners collide with the arena; touching wheels first (roof within 60° of the wall normal) lands, otherwise the body
  bounces and tips over. On the roof, jump rolls the car back on its wheels.

`tests/rlss/CarPhysicsTests.cpp` runs `Car.wren` headless: landing, acceleration curve, turning around, wall and
ceiling, jump height (~230 uu), dodge, and 400 random input sequences that must never leave the arena.

## Protocol

The transport handshake uses `protocolId = 0x524C0002` ("RL", version 2), so R-Type and RLSS peers (or two RLSS
versions) refuse each other. Every message starts with a one-byte type; multi-byte integers are big-endian, floats
IEEE 754 binary32. Received messages are untrusted: an unknown type, a wrong size, an out-of-range field or a
non-finite float drops the message.

| Type | Name          | Direction       | Channel    | Payload                                                         |
|------|---------------|-----------------|------------|-----------------------------------------------------------------|
| 1    | `LobbyState`  | server → client | Reliable   | `players u16`                                                   |
| 2    | `Snapshot`    | server → client | Unreliable | `tick u32, lastInput u32, partIndex u8, partCount u8, count u8`, then `count` × entity |
| 3    | `PlayerInput` | client → server | Unreliable | `sequence u32, steer i8 (−127..127), buttons u8 (bit 0 = jump)` |

A snapshot entity is 19 bytes: `id u32 (≠ 0), kind u8, x f32, y f32, rotation f32, flags u8 (bit 0 = facing left),
color u8`. A part carries at most 48 entities, a snapshot at most 64 parts.

## Not done yet

- No ball, no goals, no boost.
- Cars do not collide with each other.
- No client-side prediction: the local car is interpolated like the others (fine on a LAN).
