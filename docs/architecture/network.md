# Network Architecture — Developer Guide

This document explains how the networking layer is designed, why it is designed this way, and how the server and the
client use it.

It covers the **architecture** of the network module of the engine: the transport, and how it plugs into the ECS. Related
pages:

- [Add the Network to a Game](../getting-started/networked-game.md): the checklist for a game using this module.

- [Network Transport & Connection](../protocol/transport.md): which messages use which channel, connection lifecycle,
  framing and validation.
- [Protocol RFC](../protocol/rfc.txt) and [Packets](../protocol/packets.md): byte layout of each packet.
- [Network comparative study](../comparative-study/network.md): why Asio and why this architecture.

---

## Table of contents

1. [Requirements](#1-requirements)
2. [Overview](#2-overview)
3. [Library layout](#3-library-layout)
4. [Public API](#4-public-api)
5. [Internal design](#5-internal-design)
6. [Threading model](#6-threading-model)
7. [Plugging into the engine](#7-plugging-into-the-engine)
8. [Replication](#8-replication)
9. [Using the transport directly](#9-using-the-transport-directly)
10. [Pitfalls](#10-pitfalls)

---

## 1. Requirements

| Requirement                               | Impact on the network layer                                     |
|-------------------------------------------|-----------------------------------------------------------------|
| Real-time multiplayer game                | Gameplay traffic must have low and stable latency               |
| Authoritative server                      | Clients send inputs; the server sends the resulting world state |
| Asio is mandatory for server networking   | The library is built on Asio                                    |
| Linux and Windows support                 | No direct OS socket API in the codebase                         |
| Separate client and server executables    | One shared library, used by both                                |
| Network code must not leak into game code | Game code depends on interfaces only                            |

---

## 2. Overview

```
+------------------------+                        +------------------------+
|      Game client       |                        |      Game server       |
+-----------+------------+                        +-----------+------------+
            | INetworkClient                                  | INetworkServer
+-----------v------------+                        +-----------v------------+
|     NetworkClient      |   TCP  — Reliable      |     NetworkServer      |
|                        | <====================> |                        |
|                        |   UDP  — Unreliable    |                        |
|                        | <--------------------> |                        |
+------------------------+                        +------------------------+
```

Three ideas drive the design:

1. **Two transports, one API.** Game code chooses a *channel* (`Reliable` or `Unreliable`); the library maps it to TCP
   or UDP. Which message uses which channel is defined
   in [Network Transport & Connection](../protocol/transport.md#1-transport-tcp-or-udp).
2. **Interfaces only.** Game code never sees Asio, sockets or endpoints, only `PeerId`s and bytes.
3. **Polling.** The library receives on its own thread and queues events; game code pulls them once per tick.

---

## 3. Library layout

The network code is part of the engine, split between xmake targets, so that a game using the engine gets a
transport without depending on R-Type code:

| Target                | Kind           | Folder                 | Content                                                      | Depends on                      |
|-----------------------|----------------|------------------------|--------------------------------------------------------------|---------------------------------|
| `engine_network`      | static library | `engine/src/net/`      | Interfaces, plugins, replication systems, `NetworkConfig`    | `engine_core`                   |
| `engine_network_asio` | static library | `engine/src/net/asio/` | Asio implementation of the interfaces, `createServer/Client` | `engine_network`, `log`, `asio` |
| `engine_network_metrics` | static library | `engine/src/net/metrics/` | `NetworkMetricsPlugin`: network lines of the [metrics](metrics.md) board | `engine_network`, `engine_metrics` |

```
engine/src/net/                      # target "engine_network": must not include Asio
├── INetworkManager.hpp              # PeerId, Bytes, Channel, EventType, State, NetworkEvent, INetworkManager
├── INetworkServer.hpp               # ecs::net::INetworkServer
├── INetworkClient.hpp               # ecs::net::INetworkClient
├── NetworkConfig.hpp                # NetworkConfig, isValid(), MAX_UNRELIABLE_PAYLOAD
├── NetworkFactory.hpp               # createServer() / createClient(), defined in engine_network_asio
├── NetworkPlugin.hpp/.cpp            # transport events -> EventBus, registers INetworkService
├── NetworkEvents.hpp                 # PeerConnected, PeerDisconnected, PacketReceived
├── INetworkService.hpp               # what systems send through, in the Mediator
├── INetworkEventSource.hpp           # where NetworkPlugin reads its events from
├── ClientConnection.hpp/.cpp         # client transport -> service + event source
├── PeerGroupConnection.hpp/.cpp      # server transport shared by several engines -> service + event source
├── ServerReplicationPlugin.hpp/.cpp  # inputs in, snapshots out
├── ClientReplicationPlugin.hpp/.cpp  # snapshots in, inputs out, SnapshotState
├── ReplicationSystems.hpp/.cpp       # the systems, as free functions
├── ReplicationEvents.hpp             # InputApplied, LocalInput, ReplicatedEntitySpawned / Despawned
├── IReplicationCodec.hpp             # wire format, provided by the game
├── SnapshotApplier.hpp/.cpp          # what a codec writes a snapshot through
├── InputQueue.hpp/.cpp               # pushInput / popInput on an InputBuffer
└── NetworkEntityMap.hpp/.cpp         # network id <-> local entity
engine/src/net/metrics/              # target "engine_network_metrics"
└── NetworkMetricsPlugin.hpp/.cpp     # Down, Up, Packets, RTT, Missed on the metrics board
engine/src/net/asio/                 # target "engine_network_asio": Asio is used only here (namespace ecs::net::detail)
├── NetworkServer.hpp/.cpp
├── NetworkClient.hpp/.cpp
├── TcpSession.hpp/.cpp              # shared by NetworkServer and NetworkClient
├── Wire.hpp                         # bytes the library adds on the wire (kinds, headers)
├── EventQueue.hpp/.cpp
└── NetworkFactory.cpp
```

Every target exports `engine/src/` as a public include directory, so a header is included with its folder:

```cpp
#include "net/INetworkServer.hpp"
```

### Why two targets

- `engine_network` contains abstract classes, plain types, plugins and systems, all free of Asio. Code that depends on it
  (game logic, tests using a mock network) never pulls in Asio.
- `engine_network_asio` is the only target linked against Asio. Changing the implementation (another transport, another
  library) does not touch `engine_network` or the game code. Only the executables that open sockets link it.
- `engine_network_metrics` is apart for the same reason: `engine_network` does not depend on the metrics, and only a
  program that shows them (the client) links it.

```
r-type_server ──┐
                ├──► engine_network_asio ──► engine_network
r-type_client ──┘       │
                        └──► asio
```

### Nothing game-specific

The transport takes everything that depends on the game from a `NetworkConfig` given at creation, so a second game
reuses it as is:

```cpp
auto server = ecs::net::createServer({.protocolId = MY_GAME_PROTOCOL_ID, .maxPeers = 16});
```

| Field                   | Default | Meaning                                                                     |
|-------------------------|---------|-----------------------------------------------------------------------------|
| `protocolId`            | `0`     | Checked in the transport handshake: a peer of another game is refused       |
| `maxPeers`              | `64`    | Server: simultaneous connections, handshaking ones included                 |
| `udpTick`               | 250 ms  | Period of the UDP housekeeping (timeouts, keepalives, bind resends)         |
| `keepaliveInterval`     | 1 s     | Empty datagram sent after this much UDP silence                             |
| `udpTimeout`            | 5 s     | While UDP is open, a peer silent for this long is disconnected              |
| `handshakeTimeout`      | 5 s     | Server: a connection that did not send its `Handshake` in time is closed    |
| `maxReliablePayload`    | 64 KiB  | Largest TCP payload, sent or received                                       |
| `maxReliableQueueBytes` | 1 MiB   | Bytes waiting to be written to one peer before it is considered not reading |

`createServer()` / `createClient()` throw `std::invalid_argument` if `isValid(config)` is false (zero duration or limit,
keepalive not shorter than the timeout). The UDP datagram size (1200 bytes, `MAX_UNRELIABLE_PAYLOAD` = 1192 for the
game)
is not configurable: it is chosen to avoid IP fragmentation, not by the game. The port is not part of the configuration
either: it is passed to `start()` / `connect()`, and its default value belongs to the game (`protocol::DEFAULT_PORT`).

### Layers

```
Game code (server / client)
        │  game messages
Protocol layer      header, serialization, message types (see RFC)
        │  Bytes
Network library     TCP / UDP, sessions, client identification, events
        │
Asio
        │
OS sockets (epoll / IOCP / kqueue)
```

The network library transports **bytes only**. It knows nothing about game messages. The protocol layer sits on top of
it.

---

## 4. Public API

### Types

Declared in `engine/src/net/INetworkManager.hpp`, next to `INetworkManager`.

```cpp
namespace ecs::net {

using PeerId   = uint32_t;
using Bytes    = std::vector<uint8_t>;

enum class Channel   { Reliable, Unreliable };
enum class EventType { Connected, Disconnected, Data };

struct NetworkEvent {
    EventType type;
    PeerId    peer;      // starts at 1 on the server, always 0 on the client side
    Channel   channel;   // meaningful for Data events
    Bytes     data;      // empty for Connected / Disconnected
};

}
```

Event contract, the same on both sides:

| Event          | Meaning                                                                                                                                       |
|----------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| `Connected`    | The library is ready for this connection. Server: `Handshake` accepted. Client: `Welcome` received, `openUdp()` can be called.                |
| `Data`         | A game message. `channel` tells whether it came over TCP or UDP.                                                                              |
| `Disconnected` | The connection is gone (closed, timed out, kicked, or the peer stopped reading). Sent once per connection. Client: the client is now stopped. |

On the client, a `Disconnected` can arrive **without** a `Connected` before it, when a connection fails before the
library is ready: server unreachable or full, other `protocolId`, or connection lost before `Welcome`. Game code must
handle a `Disconnected` for a connection it never saw, and `disconnectReason()` tells it why. On the server, a peer is announced only once its handshake is accepted, so
a refused connection produces no event at all.

The server and the client are **single-use objects**. After `stop()`, their `io_context` cannot run again:
`start()` / `connect()` return `false`. To reconnect (back to the menu, then join again), create a new object. A failed
`start()` (port taken) can be retried on the same object. `connect()` only reports its failure later, with `Disconnected`:
that client is stopped, so create another one to retry.

On the client, losing the connection also stops it. When its TCP session closes (server gone, kicked, UDP timeout,
protocol violation), the client switches to `Stopped` **before** pushing `Disconnected`, so when the game reads that
event, `isRunning()` already returns `false`, `openUdp()` and `connect()` return `false`, and `send()` does nothing.
The network thread still runs until `stop()` (or the destructor) joins it; calling `stop()` then, or several times, is
safe.

### Interfaces

A function goes into `INetworkManager` only if it has **the same signature and meaning** on both sides.

```cpp
class INetworkManager {
public:
    virtual ~INetworkManager() = default;
    virtual std::vector<NetworkEvent> poll() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

class INetworkServer : public INetworkManager {
public:
    virtual bool start(uint16_t port) = 0;
    virtual void send(PeerId peer, const Bytes& data, Channel channel) = 0;
    virtual void broadcast(const Bytes& data, Channel channel) = 0;
    virtual void kick(PeerId peer) = 0;
    virtual void refuse(PeerId peer, DisconnectReason reason);  // tells the client why, then closes
};

class INetworkClient : public INetworkManager {
public:
    virtual bool connect(const std::string& host, uint16_t port) = 0;  // never blocks
    virtual DisconnectReason disconnectReason() const = 0;
    virtual bool openUdp(uint16_t port) = 0;
    virtual void closeUdp() = 0;
    virtual void send(const Bytes& data, Channel channel) = 0;
};
```

```
          INetworkManager          poll · stop · isRunning
           /            \
  INetworkServer      INetworkClient
  start · send(id)    connect · disconnectReason
  broadcast · kick    openUdp · closeUdp · send
  refuse
           |                 |
    NetworkServer      NetworkClient       (private)
```

### Factory

```cpp
std::unique_ptr<INetworkServer> ecs::net::createServer(const NetworkConfig& config = {});
std::unique_ptr<INetworkClient> ecs::net::createClient(const NetworkConfig& config = {});
```

Game code never includes an implementation header, so it never includes Asio. The factory is declared in
`engine_network` and defined in `engine_network_asio`: link the latter to use it.

---

## 5. Internal design

### Server

```
NetworkServer                         implements INetworkServer
├── asio::io_context + std::thread    single network thread (TCP and UDP)
├── EventQueue
├── tcp::acceptor
├── udp::socket                       one socket for all clients
├── steady_timer                      UDP tick (udpTick): handshake and UDP timeouts, keepalives
└── unordered_map<PeerId, PeerInfo>   at most maxPeers, ids start at 1
    ├── token
    ├── shared_ptr<TcpSession>
    ├── handshaken · acceptedAt       false until a valid Handshake: the game does not know the peer yet
    ├── udpOpen                       set by UdpOpen / UdpClose received over TCP
    ├── optional<udp::endpoint>       learnt from the last valid datagram
    └── lastSeen · lastSent
```

| Class           | Responsibility                                                                                                                                                                               |
|-----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `NetworkServer` | Owns the `io_context`, the network thread and the event queue. Accepts connections (up to `maxPeers`), checks the handshake, assigns `PeerId` and token, validates datagrams, runs timeouts. |
| `TcpSession`    | One TCP connection: framing (length + kind), write queue limited to 1 MiB, deferred close. Hands each message to its owner through a callback. Inherits `enable_shared_from_this`.           |
| `EventQueue`    | Mutex-protected queue. The network thread pushes; `poll()` pops everything at once.                                                                                                          |
| `Wire.hpp`      | Message kinds, header sizes and the little-endian helpers shared by the server, the client and the session; limits come from `NetworkConfig`.                                                |

### Bytes added by the library

The library adds its own bytes around the game payload and removes them on reception. The game never sees them, and the
library never reads the game payload. The full format is in
[Network Transport & Connection](../protocol/transport.md#3-framing-and-validation).

```
TCP frame                 [ length u32 ][ kind u8 ][ payload ]
UDP client -> server      [ PeerId u32 ][ token u32 ][ payload ]
UDP server -> client      [ payload ]
```

| `kind`          | Direction       | Meaning                                                                   |
|-----------------|-----------------|---------------------------------------------------------------------------|
| `0` `Data`      | both            | Game payload, handed to the game as a `Data` event                        |
| `1` `Welcome`   | server → client | `[PeerId u32][token u32]`, needed to send UDP datagrams                   |
| `2` `UdpOpen`   | client → server | The client opened UDP: the server starts its timeout                      |
| `3` `UdpClose`  | client → server | The client closed UDP: the server stops its timeout                       |
| `4` `Handshake` | client → server | `[protocolId u32]`, first frame; any other first frame closes the session |

A message with a kind not allowed in its direction closes the session. An empty UDP payload is a keepalive.

### Closing a session

`TcpSession::close()` never closes immediately. It marks the session as closing, so nothing more is read, handed to the
owner or queued, and posts the real close to the network thread. The owner's close handler (which erases the peer from
`_peers` on the server) therefore never runs in the middle of one of the owner's own loops, such as `broadcast()`
iterating over `_peers` while one peer's write queue overflows. Every caller can call `close()` anywhere.

`stop()` still waits for these closes: a posted handler is pending work, so `io_context::run()` executes it before
returning.

### Resource limits

| Limit                       | Value  | When it is exceeded                                                      |
|-----------------------------|--------|--------------------------------------------------------------------------|
| Peers per server            | 64     | The new TCP connection is closed right away (`maxPeers`)                 |
| Handshake delay             | 5 s    | A connection that did not send a valid `Handshake` is closed             |
| TCP write queue per session | 1 MiB  | The peer is not reading anymore: the session is closed (`Disconnected`)  |
| TCP payload                 | 64 KiB | `send()` refuses it; a received frame announcing more closes the session |
| UDP game payload            | 1192 B | `send()` refuses it (and refuses an empty one, reserved for keepalives)  |

Every value except the UDP payload is a `NetworkConfig` default. `maxPeers` protects the server's resources. It is not
the number of players per room, which is game logic.

### Routing

```cpp
void NetworkServer::send(PeerId client, const Bytes& data, Channel channel) {
    if (channel == Channel::Reliable)
        tcp_.send(client, data);
    else
        udp_.send(client, data);
}
```

This is the only place where a channel is mapped to a transport. Replacing TCP with a reliable channel over UDP would
only change this class.

### Client

```
NetworkClient                         implements INetworkClient
├── asio::io_context + std::thread    single network thread (TCP and UDP)
├── EventQueue
├── shared_ptr<TcpSession>            connection to the server, same class as on the server
└── udp::socket                       opened by openUdp(), closed by closeUdp()
```

The client reuses `TcpSession` instead of reimplementing TCP: framing, size check, write queue and close logic exist
only once, so client and server cannot drift apart. The client's events always carry `PeerId` 0. It pushes
`Connected` when `Welcome` arrives, and its close handler closes UDP and pushes `Disconnected`.

| Function        | Effect                                                                                                                                                                                                                        |
|-----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `connect()`     | Starts the network thread and returns at once. There, resolves the name and opens TCP only, creates the `TcpSession`, sends `Handshake`. `Connected` follows when `Welcome` (`PeerId` + token) arrives, `Disconnected` if it fails. |
| `openUdp(port)` | Called at game start with the server's UDP port. Returns `false` before `Connected`. Opens UDP towards the TCP server's address, sends `UdpOpen` over TCP, then an empty bind datagram every 250 ms until the server answers. |
| `closeUdp()`    | Called at game over. Sends `UdpClose` over TCP and closes UDP. The client goes back to TCP only (lobby).                                                                                                                      |

---

## 6. Threading model

```
Network thread (Asio)                     Game thread (fixed tick)
  io_context.run()                          each tick:
  async receive TCP / UDP                     1. events = network.poll()
  → push NetworkEvent ───── EventQueue ──►    2. handle events
                                              3. update simulation
  perform send            ◄── asio::post ──   4. send / broadcast
```

| Rule                                        | Reason                                                        |
|---------------------------------------------|---------------------------------------------------------------|
| Only the network thread touches sockets     | Asio sockets are not thread-safe                              |
| `send()` / `broadcast()` use `asio::post`   | Hands the work to the network thread without locks on sockets |
| Each world has one game thread              | A simulation stays single-threaded, no locks in systems       |
| `poll()` returns all pending events at once | One lock per tick instead of one per packet                   |

---

## 7. Plugging into the engine

`NetworkPlugin` connects a transport to the engine loop. It never sees the transport itself, only two interfaces, so it
works with a fake in tests:

| Interface                       | Role                                                       | Used by                                |
|---------------------------------|------------------------------------------------------------|----------------------------------------|
| `ecs::net::INetworkEventSource` | `poll()`: the events received since the last frame         | `NetworkPlugin::preUpdate`             |
| `ecs::net::INetworkService`       | `send()`, `broadcast()`, `kick()`, resolved in the Mediator | Any system, without knowing the plugin |

| Stage       | What the plugin does                                                                                       |
|-------------|------------------------------------------------------------------------------------------------------------|
| `init`      | Registers the service, creates the `NetworkEntityMap` resource, listens for killed entities to free their id |
| `preUpdate` | Publishes each event on the `EventBus`: `PeerConnected`, `PeerDisconnected`, `PacketReceived`              |
| `shutdown`  | Removes the listener and the service                                                                       |

Two adapters provide both interfaces:

- **`ClientConnection`** wraps a client transport. The only peer is `SERVER_PEER`.
- **`PeerGroupConnection`** gives one engine a part of a server transport. A transport has one event queue for all its
  peers, so the code that owns it polls it once and routes each event with `push()`. In the other direction, `broadcast()`
  sends to the peers of the group and never calls the transport's own `broadcast()`, which would reach every group.

---

## 8. Replication

The server is authoritative: clients send inputs, and it sends back what happened. `engine_network` handles when to send,
to whom, and what a client does with the result. The game provides the bytes (`IReplicationCodec`) and the meaning of an
input (`InputApplied`), so the engine knows nothing about the game.

Plugins require `NetworkPlugin` instead of being a mode of it, so that the transport stays usable without replication.

### Components

| Component      | Side   | Role                                                                                    |
|----------------|--------|-----------------------------------------------------------------------------------------|
| `NetworkId`    | Both   | Id shared by every machine: local entity numbers differ and are recycled                |
| `Replicated`   | Both   | The entity is in the snapshots; `kind` (32 bits) is defined by the game                 |
| `NetworkOwner` | Server | Peer that controls the entity and receives snapshots                                    |
| `InputBuffer`  | Server | Inputs not applied yet, sorted, deduplicated and bounded; `lastProcessed` is sent back  |
| `Predicted`    | Client | Local entity: moved at once, then reconciled; keeps the unacknowledged inputs           |
| `Interpolated` | Client | Other entities: their `Transform` moves between the last two snapshots                  |

### Server, one frame

```
preUpdate    NetworkPlugin publishes PacketReceived
               -> codec.decodeInput, then pushInput in the InputBuffer of every entity the sender owns
fixedUpdate  (0 to N times) popInput on each InputBuffer -> InputApplied -> the game moves the entity
update       if a step ran: writeEntity for each NetworkId + Replicated entity,
             then encodeSnapshot(lastProcessed) and send() of each part for each owning peer, over UDP
```

One input per entity and per step, like the client's prediction, so both sides advance at the same pace. Snapshots are
sent in `update`, after every fixed step of the frame: several catch-up steps give one snapshot.

### Client, one frame

```
preUpdate    NetworkPlugin publishes PacketReceived
               -> codec.decodeSnapshot; nothing if invalid, older than the last snapshot, or a part already received
               -> codec.applySnapshot, through the SnapshotApplier
               -> ReplicatedEntitySpawned for the new entities
               -> once all the parts are there: ReplicatedEntityDespawned, then kill, for the entities absent from them
               -> reconciliation of the Predicted entities of this part
fixedUpdate  the game publishes LocalInput -> encodeInput, send() after the inputs not acknowledged yet,
             InputApplied on Predicted entities
update       Interpolated entities move toward their last snapshot
```

Decoding is split in two so that a stale snapshot is dropped before it touches the registry.

### Large snapshots

A snapshot is complete, not a delta, and a datagram holds 1192 bytes. So the codec may split it: `encodeSnapshot()` returns
parts, each one holding some of the entities with the same tick, and `decodeSnapshot()` returns their `partIndex` and
`partCount` (at most `MAX_SNAPSHOT_PARTS`). The client applies a part as soon as it arrives. It only removes the entities
absent from a snapshot once it has all its parts, because a lost part must not remove what it carried. If parts keep being
lost, dead entities stay until a snapshot is complete, while the others keep moving.

### Lost inputs

An input the server never receives is one step of movement the client predicted and the server did not apply: the next
snapshot pulls the entity back by that much. So each datagram is followed by the last `INPUT_RESEND_COUNT` (2) inputs not
acknowledged yet, which costs three times the packets while the round trip lasts. The server ignores the ones it already
has (`pushInput()`), so an input is never applied twice.

### Prediction and reconciliation

Waiting for the server would add a round trip of delay to every key press, so the local entity moves at once. Then each
snapshot corrects it. With inputs moving a ship by 1:

1. The client sends and applies inputs 1, 2 and 3: locally, `x` = 3.
2. The server has only received input 1: its snapshot says `x` = 1, `lastProcessedInput` = 1.
3. The client sets `x` to 1, drops input 1 from the history, and replays 2 and 3 (`InputApplied{.replayed = true}`): `x` = 3.

If the server computed something else, the ship ends up where it should without any special case. This only works if
**both sides run the same function** on `InputApplied`: R-Type shares `applyShipInput` through `rtype_game`.

### Interpolation

Snapshots arrive irregularly, and some are lost. For an `Interpolated` entity, a snapshot does not overwrite the
`Transform`: what is displayed becomes `from`, the snapshot becomes `to`, and the entity moves between them over
(ticks since the previous snapshot) × `Engine::fixedDeltaTime()`. This assumes that the server and the client use the same
fixed step. The first snapshot is applied at once, so that an entity does not slide in from the origin.

### Safety

- Events are published **after** iterating over the components, never during it: a handler may spawn or kill entities.
- The codec validates everything it decodes, and an id of 0 is refused by `SnapshotApplier::entity()`.
- A part larger than `MAX_UNRELIABLE_PAYLOAD` is not sent. `partCount` and `partIndex` come from the network: a part that
  announces an impossible value is dropped before it touches the registry.

---

## 9. Using the transport directly

The plugins above are the normal way to use the network. The transport can also be used alone, without an engine.

### Server

```cpp
class GameServer {
public:
    explicit GameServer(ecs::net::INetworkServer& network) : network_(network) {}

    void tick() {
        for (auto& e : network_.poll()) {
            switch (e.type) {
                case ecs::net::EventType::Connected:    onJoin(e.peer);  break;
                case ecs::net::EventType::Disconnected: onLeave(e.peer); break;
                case ecs::net::EventType::Data:         onPacket(e.peer, e.channel, e.data); break;
            }
        }
        simulate();
        network_.broadcast(serializeWorldState(), ecs::net::Channel::Unreliable);
    }

private:
    ecs::net::INetworkServer& network_;
};

int main() {
    auto network = ecs::net::createServer({.protocolId = MY_GAME_PROTOCOL_ID});
    if (!network->start(4242))
        return 1;
    GameServer server(*network);
    // fixed-timestep loop calling server.tick()
}
```

### Client

```cpp
auto network = ecs::net::createClient({.protocolId = MY_GAME_PROTOCOL_ID});
network->connect("127.0.0.1", 4242);  // returns at once: wait for Connected, or Disconnected if it fails

// each tick
network->send(serializeInput(sequence, keys), ecs::net::Channel::Unreliable);
for (auto& e : network->poll()) {
    if (e.type == ecs::net::EventType::Data)
        handlePacket(e.channel, e.data);
    else if (e.type == ecs::net::EventType::Disconnected)
        showConnectionLost();
}
```

### Testing without a network

Because game code depends on interfaces only, a `MockNetworkServer` returning scripted events can test server logic
without sockets.

---

## 10. Pitfalls

| Pitfall                                                                             | Consequence                                       | Fix                                                                      |
|-------------------------------------------------------------------------------------|---------------------------------------------------|--------------------------------------------------------------------------|
| Asio included in a public header                                                    | Slow builds, `<windows.h>` conflicts in game code | Asio only in `engine/src/net/asio/`, never in `engine/src/net/*.hpp`     |
| Windows: UDP receive fails with `connection_reset` after sending to a closed client | Receive loop stops; server deaf to everyone       | Ignore `connection_reset` / `connection_refused` and restart the receive |
| Callback uses a destroyed `TcpSession`                                              | Crash                                             | `shared_from_this()` in every async handler                              |
| Socket used from the game thread                                                    | Data race                                         | `asio::post` to the network thread                                       |
| Receive buffer smaller than the datagram                                            | Datagram silently truncated                       | Buffer ≥ maximum packet size                                             |
| `system_clock` for timeouts                                                         | Timeouts break when the system clock changes      | `steady_clock`                                                           |
| Identifying clients by IP only                                                      | Players behind the same NAT are merged            | `PeerId` + token in every client datagram                                |
| UDP timeout while the client is in the lobby                                        | Every lobby player kicked after 5 s               | Timeout only while `udpOpen`, driven by `UdpOpen` / `UdpClose` over TCP  |
| Timeout relying on the game sending every tick                                      | A pause screen disconnects everyone               | Keepalive sent by the library after 1 s without UDP traffic              |
| Async send buffer not captured by the handler                                       | Asio reads freed memory                           | Capture the `shared_ptr` holding the buffer in the handler               |
| Closing a session inside a loop over `_peers` (e.g. queue full during `broadcast`)  | Iterator invalidated, undefined behaviour         | `TcpSession::close()` is always deferred with `asio::post`               |
| Peer that never reads its TCP socket                                                | Server memory grows with every `send`             | Write queue limited to 1 MiB, then the session is closed                 |
| Connection flood                                                                    | Thousands of idle sessions                        | `maxPeers`, and `handshakeTimeout` for connections that stay silent      |
| Client of another game (or a port scanner) connecting                               | Garbage reaches the game's decoder                | `Handshake` with `protocolId` checked before the game sees the peer      |
| Windows: datagram bigger than the receive buffer                                    | `message_size` error logged for each packet       | Ignore `message_size` like `connection_reset` and restart the receive    |
| `start()` / `connect()` after `stop()`                                              | Returns `true` but nothing runs                   | Three-state `Idle` / `Running` / `Stopped`, single-use objects           |
| Publishing an event while iterating over components                                 | A handler killing an entity skips others or crashes | Collect during `runSystem`, publish after                               |
| Different code moving the entity on the client and the server                       | The predicted entity jumps at every snapshot      | One function on `InputApplied`, shared by both sides                     |
| One server transport `broadcast()` used by a room                                   | The snapshot reaches the players of other rooms   | `PeerGroupConnection` sends to its own peers only                        |
| Payloads pushed to an engine that does not advance                                  | The queue grows without limit                     | Drop them until it runs, like `Room::onPacket`                           |
| Codec shared between engines                                                        | A snapshot mixes the entities of two worlds       | One codec instance per engine                                            |
