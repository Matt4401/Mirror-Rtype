# Add the Network to a Game

How a game built on the engine gets connections, inputs, replication, prediction and interpolation without writing any
socket code. This page is the checklist for a new game; R-Type follows it, see `game/src/`, `server/src/Room.cpp` and
`client/src/net/`. The design is explained in [Network Architecture](../architecture/network.md).

A game provides three things: **a wire format**, **the plugins** and **`Replicated` entities**. Everything else is engine
code.

## 1. Write a codec

`ecs::net::IReplicationCodec` translates between the ECS and bytes. It is the only code that knows your protocol.

| Side   | Method                                 | Does                                                                 |
|--------|----------------------------------------|----------------------------------------------------------------------|
| Server | `beginSnapshot(tick)`                  | Forgets the entities of the previous snapshot                        |
| Server | `writeEntity(registry, entity, id, r)` | Adds one entity; reads the components you want to send               |
| Server | `encodeSnapshot(lastProcessedInput)`   | Returns the parts of the snapshot; called once per peer              |
| Server | `decodeInput(bytes)`                   | Returns the input, or nothing if the payload is not a valid one      |
| Client | `encodeInput(input)`                   | Returns the bytes of a local input                                   |
| Client | `decodeSnapshot(bytes)`                | Keeps the part; returns its tick, acknowledged input and part number |
| Client | `applySnapshot(applier)`               | Writes each entity through the `SnapshotApplier`                     |

```cpp
void MyCodec::applySnapshot(ecs::net::SnapshotApplier& applier) {
    for (const MyEntity& state : _received) {
        // Finds the local entity of this network id, or creates it. Entities that no part of a snapshot passes here are
        // removed.
        if (const auto entity = applier.entity(state.id, state.kind)) {
            applier.setTransform(*entity, ecs::Transform{.x = state.x, .y = state.y});
        }
    }
}
```

Three rules:

- Everything decoded comes from the network: validate it, and return nothing instead of throwing.
- Each part must fit in `MAX_UNRELIABLE_PAYLOAD` bytes (1192); a larger one is not sent. Split a large snapshot into at
  most `MAX_SNAPSHOT_PARTS` parts, each with its `partIndex` and `partCount`. Always return at least one part, even for an
  empty world: that is how a client learns that its entities are gone.
- A codec keeps state between calls: give each engine its own instance.

`setTransform` replaces the whole `Transform`. If your wire format only carries a position, start from the current
`Transform`, or the client loses the scale it set on its sprites (see `game/src/game/ReplicationCodec.cpp`).

## 2. Add the plugins

Pick a `protocolId` for your game: a peer announcing another one is refused during the transport handshake.

### Client

```cpp
const std::shared_ptr<ecs::net::INetworkClient> transport = ecs::net::createClient({.protocolId = MY_GAME_ID});
const auto connection = std::make_shared<ecs::net::ClientConnection>(transport);

engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection)
    .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<MyCodec>());

transport->connect(host, port);  // returns at once: PeerConnected follows, or PeerDisconnected if it fails
```

After `PeerDisconnected`, `transport->disconnectReason()` tells why: server unreachable or full, other protocol.

Call `transport->openUdp(port)` when your game starts: snapshots and inputs travel over UDP.

### Server

A transport has one event queue for all its peers. A server with several worlds polls it once and routes the events; each
world gets a `PeerGroupConnection`, which also stops it from reaching the peers of another world.

```cpp
auto connection = std::make_shared<ecs::net::PeerGroupConnection>(*server);  // the transport must outlive it

engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection)
    .addPlugin<ecs::net::ServerReplicationPlugin>(std::make_shared<MyCodec>());
engine.init();

// In your own loop:
for (auto& event : server->poll()) {
    if (event.type == ecs::net::EventType::Connected) {
        connection->addPeer(event.peer);
    } else if (event.type == ecs::net::EventType::Disconnected) {
        connection->removePeer(event.peer);
    } else {
        connection->push(std::move(event));
    }
}
engine.advance();  // runs the steps that are due, then sends one snapshot if any ran
```

`push()` only queues events; `poll()` empties the queue during `advance()`. Do not push for an engine you are not advancing.

## 3. Replicate entities

### Server: what clients see, and who controls what

```cpp
const ecs::Entity ship = registry.spawnEntity();
ecs::net::replicate(registry, ship, MY_SHIP_KIND);   // network id + Replicated: it is now in the snapshots
registry.addComponent(ship, ecs::NetworkOwner{peer});  // this peer receives snapshots and controls the entity
registry.addComponent(ship, ecs::InputBuffer{});       // its inputs are queued here
registry.addComponent(ship, ecs::Transform{});
```

`kind` is yours: it tells the client what the entity is. Only peers owning an entity receive snapshots.

### Both sides: the same movement

The engine does not know what an input means. It publishes `InputApplied`: on the server for each input it takes from a
buffer, on the client when an input is predicted and again when it is replayed. Subscribe **the same function** on both
sides, or the prediction will not match the server.

```cpp
engine.eventBus().subscribe<ecs::net::InputApplied>([&](const ecs::net::InputApplied& event) {
    moveShip(registry.getComponents<ecs::Transform>().get(event.entity), event.input, event.deltaTime);
});
```

`event.replayed` is true during a replay: do not play sounds or effects again.

### Client: send inputs, and choose how entities follow the server

```cpp
// Once per fixed step, with a sequence that always increases:
engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = ++sequence, .held = held, .axes = axes}});

// The local entity predicts, the others are smoothed between snapshots:
engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>([&](const ecs::net::ReplicatedEntitySpawned& event) {
    if (isLocal(event)) {
        registry.addComponent(event.entity, ecs::Predicted{});
    } else {
        registry.addComponent(event.entity, ecs::Interpolated{});
    }
    addSprite(event.entity, event.kind);
});
```

`ReplicatedEntityDespawned` is published just before an entity disappears, to play an explosion for instance.

## Testing without sockets

Plugins only see `INetworkEventSource` and `INetworkService`, so a fake transport is enough, and no window is needed.
`tests/test_replication.cpp` runs a full server to client round trip with a small binary codec and no protobuf:
start from it to test your own codec.
