# Client Architecture

## 1. Overview

The R-Type client executable (`r-type_client`) is built on the generic game engine. It does not simulate the game: the
server does. The client sends the player's input, predicts the local ship, and displays what the server's snapshots say.

```mermaid
flowchart TD
    Main["client/src/main.cpp"] --> Engine["engine::Engine"]
    Engine --> RenderPlugin["render::RenderPlugin (Raylib)"]
    Engine --> InputPlugin["input::InputPlugin"]
    Engine --> NetworkPlugin["ecs::net::NetworkPlugin"]
    Engine --> Replication["ecs::net::ClientReplicationPlugin"]
    Engine --> NetPlugin["client::RTypeNetworkPlugin"]
    Engine --> GamePlugin["client::RTypeGamePlugin"]

    NetPlugin -->|"session, inputs, Predicted / Interpolated"| Replication
    Replication -->|"ReplicatedEntitySpawned"| GamePlugin
    GamePlugin -->|"sprites"| RenderPlugin
    InputPlugin -->|"InputState"| NetPlugin
```

---

## 2. Entry Point (`main.cpp`)

`r-type_client [host] [port]`, by default `127.0.0.1` and port 4242. It exits with code 84 if the arguments are invalid or
the server cannot be reached. Once connected, it builds the engine:

```cpp
const std::shared_ptr<ecs::net::INetworkClient> transport =
    ecs::net::createClient({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID});
if (!transport->connect(options->host, options->port)) {
    // error message, exit code 84
}
const auto connection = std::make_shared<engine::ClientConnection>(transport);

engine::Engine engine;
engine.addPlugin<render::RenderPlugin>()
    .addPlugin<input::InputPlugin>()
    .addPlugin<systems::PhysicsPlugin>()
    .addPlugin<systems::AnimationPlugin>()
    .addPlugin<prefabs::PrefabPlugin>()
    .addPlugin<scripting::ScriptPlugin>()
    .addPlugin<ecs::net::NetworkPlugin>(connection, connection)
    .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rtype::game::ReplicationCodec>())
    .addPlugin<client::RTypeNetworkPlugin>(transport)
    .addPlugin<client::RTypeGamePlugin>();

engine.run();
```

---

## 3. The Game Plugins

The client splits what needs a window from what does not, so that the network logic runs in tests.

### `RTypeNetworkPlugin`: the session with the server

No window and no rendering (`client/src/net/`, tested by `test_client_network` and `test_server_e2e`).

- **Session:** sends `Hello` when connected. On `LobbyList`, while it is in no room, it joins the first room waiting for
  players, or creates one. If the room started in the meantime, the join is refused, and the server pushes a new list
  because the room changed: the client tries again from it.
- **Ready:** the `ready` action (key R) toggles the ready state while in a room.
- **Game start:** `GameStart` gives the player's index and opens UDP on the port received in `Welcome`.
- **In game:** once per fixed step, publishes `ecs::net::LocalInput` with the actions held and the axes of the local
  `InputState`. The `ready` action is not a game input and is not sent.
- **Replicated entities:** the ship whose `subtype` is the player's index becomes `ecs::Predicted`, and every other entity
  `ecs::Interpolated`. On `InputApplied`, it moves the entity with `rtype::game::applyShipInput`, the same function as the
  server.
- **Connection lost:** stops the engine.

### `RTypeGamePlugin`: what is on screen

- **Initialization (`init`):** loads textures, applies the bindings (`client::input::setupDefaultBindings`), spawns the
  starfield, the camera and the entity holding the local `InputState`. It spawns no ship: ships come from the server.
- **Sprites:** on `ReplicatedEntitySpawned`, gives the entity the sprite of its `kind`, using `subtype` to choose the row of
  the ship spritesheet, so that players can tell their ships apart. A kind the client does not know stays invisible.
- **Shots (`update`):** spawns a bullet in front of the `Predicted` ship. The server does not simulate shots yet, so these
  bullets only exist on this client.

There is no lobby screen yet, because the engine cannot draw text: the state of the lobby is printed in the terminal.

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
