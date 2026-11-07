# RLSS

RLSS is the second sample game built on the engine (Track #1): a 2D, side-view car football game whose car and ball
physics are modelled on Rocket League. It proves that the engine is reusable: it lives outside `engine/`, links only the
engine modules it needs, and the engine knows nothing about it (the game does not modify `engine/` at all).

## Layout

```
rlss/
├── xmake.lua           -- includes the targets below (the client only with --graphics=y)
├── shared/             -- rlss_shared: protocol + codec, arena dimensions, player palette (no engine)
├── game/               -- rlss_game: game components, replication codec, Wren bindings (server + client)
├── server/             -- rlss_server: headless, authoritative
├── client/             -- rlss_client: window, arena and car drawing, controls
└── scripts/            -- Wren: Match, Kickoff, Car, Ball, Arena, tunings (server); CameraDirector, CarView, BallView,
                           ScoreboardView (client)
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
`rlss/scripts/CarTuning.wren` or `BallTuning.wren` while the server runs to tune the physics live.

| Action                             | Keyboard                    | Gamepad           |
|------------------------------------|-----------------------------|-------------------|
| Drive (ground) / rotate (air)      | A / D, Q / D, ← / →         | left stick        |
| Jump, then double jump or dodge    | Space, W, Z, ↑              | bottom face button |
| Boost                              | Shift, S, ↓                 | right face button, right trigger |

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
        CSP -- InputState --> MATCH[Match.wren: Car + Ball]
        MATCH -- Transform, CarStatus --> SRP
    end
    CRP -- PlayerInput, UDP --> SRP
    SRP -- Snapshot, UDP --> CRP
```

- **Server** (`rlss/server/main.cpp`): `NetworkPlugin` fed by `ServerConnection` (game-side adapter, one engine owns
  every peer), `ServerReplicationPlugin` with the RLSS `ReplicationCodec`, `ScriptPlugin`, then:
    - `CarSimulationPlugin` exposes the components to Wren, spawns the arena entity (tag `arena` + `ArenaShape`,
      with the goals, running the `Match` script, holding the `MatchState`) and the replicated ball (`Ball` script, kind
      `KIND_BALL`), copies each applied input into the car's `InputState`, and sends the `MatchState` to the clients
      (`MatchStatus` message: to everybody when it changes, to a player who joins);
    - `LobbyPlugin` gives each peer a slot (color, team, kickoff spot) and a car: `Transform`, `InputState`,
      `InputBuffer`, `NetworkOwner`, `CarLook`, `CarStatus{placeAtKickoff}`, `ScriptComponent{Car}`, replicated with
      kind `KIND_CAR`. It also broadcasts the `LobbyState` when a player joins or leaves.
- **Client** (`rlss/client/main.cpp`): connects first (no window if the server is unreachable), opens UDP, then
  `RenderPlugin`, `InputPlugin`, `NetworkPlugin` fed by `ServerLink`, `ClientReplicationPlugin`, `ScriptPlugin`, and:
    - `ArenaViewPlugin` draws the arena with flat colored rectangles;
    - `CameraPlugin` owns the camera: it keeps a `CameraRig` up to date (local car from the server's `Welcome`, ball,
      match phase, window size, arena bounds) for the `CameraDirector` script, which frames the action (see below);
    - `CarViewPlugin` gives each replicated car its parts (body and cabin in the player color, wheels, headlight,
      boost flame and gauge) and the `CarView` script that places them every frame;
    - `BallViewPlugin` builds the ball (a disc of strips and two seams, tagged `ball_part`) and the `BallView` script
      that places the pieces and turns the seams with the ball;
    - `ScoreboardPlugin` draws the score, the kickoff countdown and "GOAL" with small squares (until the engine can
      render text): it creates the squares (tagged `score_cell`, laid out in screen pixels) and writes each
      `MatchStatus` into a `MatchState`; the `ScoreboardView` script lights the squares with a 3 × 5 pixel font and
      places them through the camera every frame, so the scoreboard stays on screen while the camera moves;
    - `PlayerInputPlugin` publishes the local controls as a `LocalInput` every fixed step;
    - `SessionPlugin` logs the player count and closes the game when the connection is lost.

### What is scripted

| Wren module              | Runs on | Role                                                                     |
|--------------------------|---------|--------------------------------------------------------------------------|
| `rlss/scripts/Match`      | server  | Fixed 120 Hz loop: steps the cars and the ball, car-ball contacts, goals, score, kickoff |
| `rlss/scripts/Kickoff`    | server  | Teams, kickoff spots, goal celebration and countdown durations            |
| `rlss/scripts/Simulation` | server  | Registry the `Car` / `Ball` instances join and leave                      |
| `rlss/scripts/Car`        | server  | Whole car physics and boost, one instance per car                         |
| `rlss/scripts/Ball`       | server  | Ball physics: drag, bounces, spin                                         |
| `rlss/scripts/Arena`      | server  | Arena shape as a signed distance field: distance, wall normal, curvature  |
| `rlss/scripts/CarTuning`, `BallTuning` | server | Rocket League constants                                     |
| `rlss/scripts/CarView`    | client  | Size and placement of the car parts, wheel spin, boost flame and gauge    |
| `rlss/scripts/BallView`   | client  | Placement of the ball pieces, seams turning with the ball                 |
| `rlss/scripts/CameraDirector` | client | Camera: aim, zoom, easing, arena bounds, kickoff wide shot         |
| `rlss/scripts/ScoreboardView` | client | Score, countdown and "GOAL" drawn with squares (3 × 5 font), on screen |

The scripts see the ECS through component classes bound by `rlss::game::installScriptBindings` in the
`rlss/components` module: `Transform`, `Controls` (`InputState.playerIndex`), `ArenaShape`, `CarParts`. They read the
player input with the engine's `Input.getAxis(player, "steer")` / `Input.isDown(player, "jump" | "boost")`. Bound
classes also include `CarStatus` (boost, written by `Car`, replicated) and `BallPart` (client ball pieces).

The cars and the ball are not stepped by their own `update`: each instance registers in `Simulation`, and the `Match`
script (on the arena entity) steps them all in a fixed order, so that their contacts are resolved deterministically.

What stays in C++: creating entities (Wren can only spawn prefabs at a position), the network and replication, and
the arena drawing (hundreds of shapes created once).

## Goals and match

Like in Sideswipe, the goals open in the side walls a little above the floor: the bottom curved corner is a ramp that
ends at the goal mouth (420 uu above the floor, 360 uu high, 240 uu deep). There is no wall above the ramp to drive on:
a car driving fast up the ramp flies up the mouth or into the goal, and the walls above the goals are reached in the
air. The crossbar and the bottom of each goal meet the wall at a post, a convex edge that the ball bounces off and the
car collides with.

`Arena.wren` is an exact signed distance to the union of the rounded rectangle and the two goal boxes (nearest point
over all segments and arcs, sign from an inside test); away from the side walls it uses the faster rounded rectangle
formula alone.

- **Teams**: even player slots are blue (left side, defending the left goal, cool colors), odd slots orange (right
  side, warm colors). Teammates line up one behind the other at kickoff.
- **Goal**: as in Rocket League, once the whole ball is behind the goal line. The team attacking that goal scores.
- **After a goal**: 2 s of celebration ("GOAL" on screen, the ball stays in play), then every car goes back to its
  kickoff spot (at rest, full boost) and the ball to the center of the arena. A 3-2-1 countdown follows, everybody
  frozen, then play resumes and the ball drops. A player who joins starts at their kickoff spot.

## Camera

The camera (`CameraDirector.wren`, client) is a fixed side view, never rotated: only its position and zoom change.

1. **Aim**: a point between the local car and the ball, leaning to the ball when it is far (40% of the way when they
   are close, 60% when they are 2000 uu apart or more), so the player sees what is coming.
2. **Zoom**: the closest zoom that keeps both on screen with a 280 uu margin. Bounded: never closer than 1500 uu across
   (about 13 car lengths), never wider than the arena.
3. **Easing**: position and zoom ease toward their target every frame (rates 5/s and 2.5/s: the zoom reacts more
   slowly, in log space so that zooming in and out feel the same).
4. **Bounds**: the view never shows past the outer walls and goals; near a goal or a corner it stops against them and
   the car or the ball is no longer centered.
5. **Special cases**: wide shot on the center during the kickoff countdown; after a goal it keeps following the
   action until the kickoff; a high ball pulls the camera up and out when the car stays on the ground.
6. **The player comes first**: when the car and the ball do not fit on screen (too far apart, or while the zoom is
   still pulling back after a clearance), the aim slides back so that the car stays at least 320 uu inside the screen;
   the ball may leave it. The eased view is also held so that a fast car never gets closer than 160 uu to an edge while
   the camera catches up.

The client learns which car is its own from the server's `Welcome` message, and resolves it through the engine's
`NetworkEntityMap`.

## Car physics

Side view, y down, Rocket League units (uu). The arena is 2880 × 1536 uu with 420 uu curved corners (about 24 car
lengths wide: cars and ball keep their Rocket League size, the camera follows the action); the car hitbox is
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

### Boost

Boost pushes toward the nose: 991.7 uu/s² on the ground (where it also means full throttle), 1058.3 uu/s² in the air,
up to the 2300 uu/s cap. It burns 33.3 per second (a full tank lasts 3 s) and lasts at least 0.1 s once started. Like in
Sideswipe there are no boost pads: the tank (100, full at spawn) refills while the car touches a surface, wheels or
body. The refill rate (25 per second, empty to full in 4 s) is an estimate, not a published value.

## Ball physics

| Quantity                   | Value                                                          |
|----------------------------|----------------------------------------------------------------|
| Radius                     | 91.25 uu                                                       |
| Gravity                    | 650 uu/s²                                                      |
| Air drag                   | 3% of the speed per second                                     |
| Bounce (restitution)       | 0.6 of the speed into the wall                                 |
| Wall friction              | 0.35: sliding turns into spin and spin into sliding            |
| Max speed / spin           | 6000 uu/s / 6 rad/s                                            |
| Mass                       | a car weighs six balls                                         |

Car against ball: the rotated car hitbox is tested against the ball circle. The contact itself does not bounce
(restitution 0, friction 2, shared by mass, using the speed of the car at the contact point so a flipping nose hits
harder). Then Rocket League's extra impulse is added to the ball, at most every other step: along the line from the car
center to the ball center, its vertical part scaled by 0.35 and its part along the car nose by 0.65, with a strength of
0.65 × the relative speed up to 500 uu/s, 0.55 at 2300, 0.3 at 4600. A car hitting a ball at rest sends it about 1.3
times faster than itself; the bumper hits the ball under its center, so it also pops up.

## Tests

`tests/rlss/CarPhysicsTests.cpp` runs the Wren physics headless: landing, acceleration curve, turning around, wall and
ceiling, jump height (~230 uu), dodge, boost (past the throttle cap, 33.3 per second, refill only against a surface),
ball bounce height (0.36 × the drop), car pushing the ball, hit speed, goals (a ball behind the line scores for the
right team, celebration, kickoff spots, countdown freeze, play resuming; a shot in the goal scores, a shot above the
crossbar or under the goal does not), driving above the goal and up the ramp, and 400 random input sequences (with and
without the ball) where neither the car nor the ball may leave the arena and the goals. The test checks containment
with its own C++ distance to the arena and goals, written independently from `Arena.wren`.

`tests/rlss/CameraTests.cpp` runs `CameraDirector.wren` headless: both car and ball framed without rotation, zoom in
when close and out when far (within its bounds), aim leaning to the ball, easing instead of jumping, stopping against
the arena near a goal, wide shot at kickoff, high ball, the car kept on screen when the ball is cleared far away and
when it drives at supersonic speed, and the view never showing past the arena.

## Protocol

The transport handshake uses `protocolId = 0x524C0005` ("RL", version 5), so R-Type and RLSS peers (or two RLSS
versions) refuse each other. Every message starts with a one-byte type; multi-byte integers are big-endian, floats
IEEE 754 binary32. Received messages are untrusted: an unknown type, a wrong size, an out-of-range field or a
non-finite float drops the message.

| Type | Name          | Direction       | Channel    | Payload                                                         |
|------|---------------|-----------------|------------|-----------------------------------------------------------------|
| 1    | `LobbyState`  | server → client | Reliable   | `players u16`                                                   |
| 2    | `Snapshot`    | server → client | Unreliable | `tick u32, lastInput u32, partIndex u8, partCount u8, count u8`, then `count` × entity |
| 3    | `PlayerInput` | client → server | Unreliable | `sequence u32, steer i8 (−127..127), buttons u8 (bit 0 jump, bit 1 boost)` |
| 4    | `MatchStatus` | server → client | Reliable   | `blueScore u16, orangeScore u16, phase u8 (0 playing, 1 goal, 2 kickoff), countdown u8 (0..9)` |
| 5    | `Welcome`     | server → client | Reliable   | `carId u32 (≠ 0)`: network id of the receiver's car, sent once it exists |

A snapshot entity is 20 bytes: `id u32 (≠ 0), kind u8 (1 car, 2 ball), x f32, y f32, rotation f32, flags u8 (bit 0
facing left, bit 1 boosting), color u8, boost u8 (0..100)`. A part carries at most 48 entities, a snapshot at most 64
parts.

## Not done yet

- No match timer, no overtime, no team selection (teams follow the join order).
- Cars do not collide with each other.
- No client-side prediction: the local car is interpolated like the others (fine on a LAN).
