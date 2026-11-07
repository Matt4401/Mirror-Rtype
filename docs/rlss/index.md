# RLSS

RLSS is the second sample game built on the engine (Track #1): a 2D, side-view car football game.
It proves that the engine is reusable: it lives outside `engine/`, links only the engine modules it needs, and the
engine knows nothing about it.

## Layout

```
rlss/
├── xmake.lua           -- includes the three targets below (the client only with --graphics=y)
├── shared/             -- rlss_shared: protocol constants + message codec, command-line helpers
├── server/             -- rlss_server: headless, authoritative
└── client/             -- rlss_client: connection, game window
```

| Target             | Engine modules used                                                                 |
|--------------------|-------------------------------------------------------------------------------------|
| `rlss_shared` | none ||
| `rlss_server` | `engine_core`, `engine_time`, `engine_events`, `engine_network`, `engine_network_asio` |
| `rlss_client` | same + `engine_render`                                                              |

## How it plugs into the engine

Both executables are an `engine::Engine` plus plugins. The engine itself is not modified by the game.

- **Server**: `ecs::net::NetworkPlugin` fed by `rlss::server::ServerConnection` (game-side adapter: one engine owns
  every peer), then `rlss::server::LobbyPlugin`, which tracks the connected peers and broadcasts a `LobbyState` when
  one joins or leaves. `main.cpp` drives `engine.advance()` until `SIGINT` / `SIGTERM`.
- **Client**: `main.cpp` first connects (`rlss::client::connectToServer`); if the server cannot be reached, no window
  opens. Once connected, it starts the engine with `render::RenderPlugin` (blank window titled "RLSS"),
  `ecs::net::NetworkPlugin` fed by `rlss::client::ServerLink`, and `rlss::client::SessionPlugin`, which logs the
  player count and closes the game when the connection is lost.

Game plugins never call each other or the transport directly: they react to `PeerConnected`, `PeerDisconnected` and
`PacketReceived` on the `EventBus`, and send through the `ecs::net::INetworkService` service.

## Running

```sh
xmake
./rlss_server [port]            # default port 4250
./rlss_client [host] [port]     # default 127.0.0.1 4250, start as many as you want
```

## Protocol

The transport handshake uses `protocolId = 0x524C0001` ("RL", version 1), so R-Type and RLSS peers refuse
each other. Every message starts with a one-byte type; multi-byte integers are big-endian. Received messages are
untrusted: an unknown type or a wrong size is dropped.

| Type | Name         | Direction       | Channel  | Payload               |
|------|--------------|-----------------|----------|-----------------------|
| 1    | `LobbyState` | server → client | Reliable | `players: u16` (3 bytes total) |
