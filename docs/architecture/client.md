# Client Architecture

## 1. Overview

The R-Type client executable (`r-type_client`) is built on the generic game engine. It does not simulate the game: the
server does. The client sends the player's input, predicts the local ship, and displays what the server's snapshots say.

```mermaid
flowchart TD
    Main["client/src/main.cpp"] --> Engine["engine::Engine"]
    Main -->|"addNetworkPlugins()"| Engine
    Engine --> RenderPlugin["render::RenderPlugin (Raylib)"]
    Engine --> InputPlugin["input::InputPlugin"]
    Engine --> NetworkPlugin["ecs::net::NetworkPlugin"]
    Engine --> Replication["ecs::net::ClientReplicationPlugin"]
    Engine --> NetPlugin["client::RTypeNetworkPlugin"]
    Engine --> Control["client::PlayerControlPlugin"]
    Engine --> GamePlugin["client::RTypeGamePlugin"]
    Engine --> Scenes["client::ClientScenesPlugin (scene::ScenePlugin)"]

    Scenes -->|"ConnectRequest, JoinRoomRequest, SetReadyRequest..."| NetPlugin
    NetPlugin -->|"SessionConnected, RoomUpdated, GameStarted, LevelStarted, GameEnded..."| Scenes
    NetPlugin -->|"GameStarted, GameEnded"| Control
    Scenes -->|"setPaused (pause menu)"| Control
    Control -->|"inputs, Predicted / Interpolated"| Replication
    Replication -->|"ReplicatedEntitySpawned"| GamePlugin
    GamePlugin -->|"sprites"| RenderPlugin
    InputPlugin -->|"InputState"| Control
```

---

## 2. Entry Point (`main.cpp`)

`r-type_client [host] [port]`, by default `127.0.0.1` and port 4242 (`client/src/ClientOptions.cpp`). The address only
fills the connect screen in: nothing connects before the player asks. A host given on the command line wins over the
last server saved in the settings. The client exits with code 84 only if the arguments are invalid. `main()` only
assembles the engine:

```cpp
engine::Engine engine;
engine.addPlugin<render::RenderPlugin>(render::WindowConfig{.escapeClosesWindow = false})
    // ... input, audio, physics, animation, prefabs, scripting, hierarchy, ui ...
    .addPlugin<scene::ScenePlugin>()
    .addPlugin<scene::SceneScriptingPlugin>()
    .addPlugin<settings::SettingsPlugin>(config::userConfigDirectory("rtype") / "settings.toml")
    .addPlugin<client::RTypeGamePlugin>();
client::addNetworkPlugins(engine);
engine.addPlugin<client::ClientScenesPlugin>(options->host, options->port, args.size() > 1);
engine.run();
```

`client::addNetworkPlugins()` (`client/src/net/ClientNetwork.cpp`) adds the network plugins. `connect()` never blocks:
the window stays responsive while the client connects, and a failure arrives as a `Disconnected` event.

---

## 3. The Game Plugins

The client splits what needs a window from what does not, so that the network logic runs in tests.

### `RTypeNetworkPlugin`: the session with the server

No window and no rendering (`client/src/net/`, tested by `test_client_network` and `test_server_e2e`).

It is driven by request events and reports what happens with result events (`client/src/net/SessionEvents.hpp`), so
the screens and the session never call each other:

| Request              | Valid in        | Effect                                                   |
|----------------------|-----------------|----------------------------------------------------------|
| `ConnectRequest`     | offline         | New transport from the factory, `connect()`, then `Hello` |
| `DisconnectRequest`  | any             | `Leave`, stops the transport                             |
| `RefreshRoomsRequest`| lobby, room     | `LobbyListRequest`                                       |
| `CreateRoomRequest`  | lobby           | `LobbyCreate`                                            |
| `JoinRoomRequest`    | lobby           | `LobbyJoin`                                              |
| `SetReadyRequest`    | room            | `LobbyReady`                                             |
| `LeaveRoomRequest`   | room, game      | `Leave`; in game, closes UDP and resets the replication  |

| Result             | When                                                                         |
|--------------------|------------------------------------------------------------------------------|
| `SessionConnected` | `Welcome` received: the client is in the lobby                               |
| `SessionClosed`    | connection refused, failed or lost, with a reason for the player            |
| `RoomListUpdated`, `RoomUpdated`, `PlayerLeftRoom`, `RoomLeft` | lobby and room changes            |
| `GameStarted`      | `GameStart`: UDP opened                                                      |
| `LevelStarted`, `LevelCompleted` | `LevelStart`, `LevelComplete`                                  |
| `GameEnded`        | `GameOver`: UDP closed, replicated entities removed, back in the lobby       |

- A request that makes no sense in the current state (`SessionState`) is ignored.
- The last list of rooms, room, level and result are kept (`rooms()`, `room()`, `level()`, `lastResult()`), for a
  screen opened after the event.
- **One transport per connection.** A transport connects only once, so each `ConnectRequest` builds a new one and puts
  it in the shared `ecs::net::ClientConnection`. A failed connection can therefore be retried.
- **Clean games.** Leaving a game or receiving `GameOver` calls `ClientReplicationPlugin::reset()`. The next game,
  whose snapshot ticks start again from 1, is not dropped as stale.
- **No forced exit.** A failure never stops the engine: the scenes go back to the connect screen, which shows the
  reason.

### `PlayerControlPlugin`: the local player in game

Also without window or rendering (`client/src/net/`, same tests).

- **Inputs:** from `GameStarted` until the game ends or is left, once per fixed step, publishes `ecs::net::LocalInput`
  with the actions held and the axes of the local `InputState`, unless `setPaused(true)` (local pause). The `ready`
  action is not a game input and is not sent.
- **Replicated entities:** the ship whose `subtype` is the player's index becomes `ecs::Predicted`, and every other entity
  `ecs::Interpolated`. On `InputApplied`, it moves the entity with `rtype::game::applyShipInput`, the same function as the
  server.

### `ShipMetricsPlugin`: the game's line on the metrics board

`client/src/metrics/`, tested by `test_client_metrics`. Every `metrics::REFRESH_PERIOD`, writes the position of the
`ecs::Predicted` ship to the `Position` line of the `metrics::MetricsBoard`; there is no line before the game starts. It
requires `metrics::MetricsPlugin`, and stays out of `PlayerControlPlugin`, which only handles the player's inputs and
ship. See [In-Game Metrics](metrics.md).

### `RTypeGamePlugin`: what is on screen

- **Initialization (`init`):** loads textures, applies the bindings (`client::input::setupDefaultBindings`), spawns the
  starfield, the camera and the entity holding the local `InputState`. It spawns no ship: ships come from the server.
- **Sprites:** on `ReplicatedEntitySpawned`, gives the entity the sprite of its `kind`, using `subtype` to choose the row of
  the ship spritesheet, so that players can tell their ships apart. A kind the client does not know stays invisible.
- **Shots:** the client spawns nothing. The `shoot` action travels in the player's input, and the bullets arrive in the
  snapshots like any other entity, interpolated: a shot appears one round trip after the key press.

### Scenes: the menus (`client/src/scenes/`)

The screens are engine scenes (see [Scenes](scene.md)), built with the engine UI widgets (see
[User Interface](ui.md)) through `MenuKit`. `ClientScenesPlugin` registers them and opens the title screen.

```mermaid
stateDiagram-v2
    Title --> Connect: PLAY
    Title --> Settings: SETTINGS (push)
    Connect --> Lobby: SessionConnected
    Lobby --> Room: RoomUpdated
    Room --> Game: GameStarted
    Game --> Pause: back (push, overlay)
    Pause --> Settings: SETTINGS (push)
    Game --> LevelTransition: LevelStarted / LevelCompleted (push, Wren overlay)
    Game --> GameOver: GameEnded
    GameOver --> Lobby
    Pause --> Lobby: LEAVE (RoomLeft)
    Lobby --> Title: back (disconnects)
    Lobby --> Connect: SessionClosed
    Room --> Connect: SessionClosed
    Game --> Connect: SessionClosed
```

| Scene             | Mode           | Content                                                                         |
|-------------------|----------------|---------------------------------------------------------------------------------|
| `Title`           | exclusive      | Play, Settings, Quit                                                            |
| `Connect`         | exclusive      | host and port fields (last server saved, else the command line), progress, error |
| `Settings`        | exclusive      | master, music and effects volumes, metrics panel; saved by `SettingsPlugin`     |
| `Lobby`           | exclusive      | scrollable list of rooms (only open ones are focusable), create, refresh, back  |
| `Room`            | exclusive      | players and their ready state, ready toggle, leave                              |
| `Game`            | exclusive      | HUD (player, level, score); back opens the pause menu                           |
| `LevelTransition` | overlay        | written in Wren (`scripts/scenes/LevelTransition.wren`): spawns the C++ `level_banner` prefab, waits 2.5 s, pops itself |
| `Pause`           | overlay        | local only: the game goes on below and for the others, the ship gets no input   |
| `GameOver`        | exclusive      | victory or defeat, scores, back to the lobby                                    |

- **Navigation.** Mouse, or keyboard and gamepad through the UI focus. Escape and gamepad B mean "back": the window
  does not close on Escape, and the game quits from the title screen.
- **No missed event.** A scene entered after an event (the first `LevelStart` follows `GameStart` at once, a room
  update may arrive before the room screen opens) reads the state the session keeps.
- **One owner per action.** Click and back handlers only act while their scene is the top of the stack, so the title
  under the settings, or the game under the pause menu, never react to them.
- **Settings.** `ClientScenesPlugin` declares them, applies the volumes and the metrics panel
  (`MetricsOverlayPlugin::setVisible`) on `SettingChanged`, and toggles the metrics setting on F3. The panel is hidden
  by default.

---

## 4. Input & Rendering Pipelines

1. **Input Pipeline**:
   - `input::InputPlugin` queries hardware input state (keyboard, gamepad) once per frame in `preUpdate`, before the fixed-timestep ticks.
   - Updates the `InputState` components of the local player, read by the game plugins.
   - Dispatches transition events (`InputActionEvent`) through the `EventBus`.
2. **Rendering Pipeline**:
   - `render::RenderPlugin` collects entities possessing both `ecs::Transform` and `ecs::Renderable`.
   - Sorts draw items by layer, depth, and texture ID to minimize state changes.
   - Renders them through the camera using Raylib within `Window::beginFrame()` and `Window::endFrame()`.
