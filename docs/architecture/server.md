# Server Architecture

`r-type_server` is the authoritative side of the game: it runs the simulation and clients only send it inputs. It is
headless and does not link raylib.

## Threads

| Thread       | Runs                                       | Touches                                    |
|--------------|--------------------------------------------|--------------------------------------------|
| Network      | The Asio transport (`engine_network_asio`) | Sockets; receiving never waits on the game |
| Server       | `ServerManager::run()` and the `Lobby`     | The clients, the rooms and their players   |
| One per room | `Room::advance()`, once the game started   | The engine of this room, under its mutex   |

They only talk through the transport's event queue (`poll()`) and `asio::post` (`send()`), and a room receives its
packets
through the thread-safe `PeerGroupConnection`. A slow or crashed client therefore never blocks a game, and a room with
many
entities does not slow down the others. The mutex of a room is only contended when a player leaves it: the server thread
then waits for the step in progress. An exception in a simulation stops that room only.
See [Network Architecture](network.md#6-threading-model).

## `ServerManager`

Owns the transport and the `Lobby`, and holds no game logic. Each loop iteration, about every millisecond:

1. **`poll()` the network once** and route the events. This is the only place that reads it: a room polling the shared
   queue would steal the events of the others.
2. Kick the clients that sent no `Hello` within 5 seconds.

Reliable payloads are decoded here: the handshake is handled here too, and the rest goes to the `Lobby`. Unreliable
payloads are forwarded to the client's room without being decoded: only the replication codec reads them.

A client of another protocol version is refused with `INetworkServer::refuse()`, so that it can tell its player why.

### Lobby

`Lobby` (`server/src/Lobby.cpp`) owns the rooms and the clients past their `Hello`, and only runs on the server thread.
A room holds at most `MAX_PLAYERS_PER_ROOM` players: 2 for now, while the protocol allows up to 4.

| Message            | Effect                                                                                   |
|--------------------|------------------------------------------------------------------------------------------|
| `LobbyListRequest` | Answers with `LobbyList`                                                                 |
| `LobbyCreate`      | Creates a room with the client in it, sends `LobbyUpdate`                                |
| `LobbyJoin`        | Adds the client if the room is open, sends `LobbyUpdate` to the room                     |
| `LobbyReady`       | Updates the room; when everyone is ready, starts it and sends `GameStart` to each player |
| `Leave`            | Removes the client from its room (a disconnection does the same)                         |

- A departure sends `PlayerLeft` to the others. In a lobby it can also start the room, if the player who left was the
  last one not ready.
- An empty room is destroyed.
- Each change of the list is pushed as `LobbyList` to the clients not in a game.
- A message in the wrong phase is dropped, as the [RFC](../protocol/packets.md#8-validation) requires.
- Rooms are referred to by `RoomId`, never by pointer: an event for a room that no longer exists is ignored.

## `Room`

One game: one `engine::Engine`, so one registry, event bus, clock and set of plugins. Games are isolated from each
other.

```mermaid
flowchart LR
    SM["ServerManager"] -->|"onPacket()"| PG["PeerGroupConnection"]
    PG -->|"poll()"| NP["NetworkPlugin"]
    NP -->|"PacketReceived"| SR["ServerReplicationPlugin"]
    SR -->|"InputApplied"| MV["applyShipInput"]
    SR -->|"snapshots"| PG
    PG -->|"send() to this room's players only"| NET["INetworkServer"]
```

- **Payloads are dropped until the game starts:** nothing would consume them, and a client must not be able to fill the
  queue.
- **`start()` gives each player its index**, its position in join order, so the indices have no holes whoever left the
  lobby. `GameStart` requires `player_index < player_count`. It also resets the clock: game time starts at 0, however
  long the room waited.
- **`runInBackground()` gives a started room its thread**, which stops when the room is destroyed.
- **`advance()` does nothing while the room waits.** After several catch-up steps, a single snapshot is sent, since it
  carries the latest state.

## Collisions

The server classifies physics contacts with `rtype::server::collision::CollisionRouter` (`server/src/collision/`, target `rtype_collision`). It subscribes to the engine `events::CollisionBeginEvent` and uses the categories from `prefabs/CollisionCategories.hpp` to republish each contact as an R-Type event:

| Event | Pair |
| ------- | ------ |
| `ShotHitEvent` | Player shot ↔ enemy / obstacle (`byPlayer = true`), enemy shot ↔ player / obstacle (`byPlayer = false`) |
| `ShipContactEvent` | Player ship ↔ enemy / obstacle |
| `PowerUpTouchedEvent` | Player ship ↔ power-up |

The router does not change any state: game rules (damage, kills, pickups) subscribe to these events. `GameplayPlugin` owns the router of each room.

## Gameplay

Each room's engine runs `systems::PhysicsPlugin`, `timeline::TimelinePlugin`, `gameplay::GameplayPlugin`
(`server/src/gameplay/`, target `rtype_gameplay`), `prefabs::PrefabPlugin` and `scripting::ScriptPlugin`. Every entity
they spawn is replicated: the client only draws what the server simulates.

- **Level:** the server loads `server/levels/level_01.toml` at start-up and every room plays it. `TimelinePlugin` moves
  a cursor at the scroll speed; `GameplayPlugin` handles the events it reaches: `spawn` creates the enemies, `scroll`
  changes the speed, `end` restarts the level.
- **Scripts:** each room has its own Wren VM. What a script spawns with `Engine.spawnPrefab(name, x, y)` goes through
  `gameplay::registerScriptPrefabs()`, so it is replicated too (kind `makeEntityKind(name)`). Player ships and bullets
  cannot be spawned by a script.
- **Shots:** while a player's input holds `shoot`, its ship fires a `player_bullet` every 0.2 s at most. The bullet's
  `subtype` is the index of the player who fired it.
- **Kills and scores:** a `ShotHitEvent` of a player shot on an enemy destroys both and gives 1 point to the shooter.
  Each player has their own score.
- **Cleanup:** player bullets more than 64 px past the right edge, and enemies, enemy shots and power-ups more than
  64 px past the left one, are destroyed.
- **Score log:** every 5 s of game time, the server logs `[server] scores: P1=<n> P2=<n>`. Scores are not sent to the
  clients yet.

## What is not simulated yet

Enemy shots, ship damage, power-ups and `GameOver` are still to do, as well as `Chat`, which the server drops. A snapshot is the complete state of the room, split into as many datagrams as needed: 36 entities per
datagram, and at most 64 of them, about 2300 entities. Past that it is not sent (an error is logged).
