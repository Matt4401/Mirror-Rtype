# Packet Reference

This page describes every game-layer message. The game layer is encoded with
[Protocol Buffers](https://protobuf.dev/) (proto3, binary wire format); the schema is
[`common/proto/rtype.proto`](https://github.com/Matt4401/Mirror-Rtype/blob/main/common/proto/rtype.proto) and is the
source of truth.

For the transport framing that wraps these payloads, see [transport.md](transport.md).
For the protocol overview and connection lifecycle, see [rfc.txt](rfc.txt).

---

## Table of Contents

1. [Envelope](#1-envelope)
2. [Encoding](#2-encoding)
3. [Handshake](#3-handshake)
4. [Lobby](#4-lobby)
5. [Game Events](#5-game-events)
6. [General](#6-general)
7. [In-game (UDP)](#7-in-game-udp)
8. [Validation](#8-validation)
9. [Why Protocol Buffers](#9-why-protocol-buffers)
10. [Appendix A — R-Type Entity Conventions](#appendix-a--r-type-entity-conventions)
11. [Appendix B — Message Summary](#appendix-b--message-summary)

---

## 1. Envelope

Every game payload (a TCP `Data` frame or a UDP datagram, once the library header is stripped) is **exactly one**
serialized `Envelope`. The envelope holds a single message in a `oneof`; its field number identifies the message, like
an opcode:

```protobuf
message Envelope {
    oneof message {
        PlayerInput player_input = 1;
        WorldState world_state = 2;

        Hello hello = 16;
        Welcome welcome = 17;

        LobbyListRequest lobby_list_request = 32;
        LobbyList lobby_list = 33;
        LobbyCreate lobby_create = 34;
        LobbyJoin lobby_join = 35;
        LobbyReady lobby_ready = 36;
        LobbyUpdate lobby_update = 37;

        GameStart game_start = 48;
        GameOver game_over = 49;
        PlayerLeft player_left = 50;
        LevelStart level_start = 51;
        LevelComplete level_complete = 52;
        ScoreUpdate score_update = 53;

        Leave leave = 64;
        Chat chat = 65;
    }
}
```

The hot UDP messages use field numbers 1 and 2: their tag fits in one byte. Number ranges group messages by phase (`16`
handshake, `32` lobby, `48` game events, `64` general).

There is no magic number or opcode byte any more: protobuf parsing plus the checks of [section 8](#8-validation) reject
anything that is not a valid envelope.

---

## 2. Encoding

Standard protobuf wire format. Main consequences:

| Protobuf type | Used for                     | On the wire                                                |
|---------------|------------------------------|------------------------------------------------------------|
| `uint32`      | ids, counters, small numbers | varint: 1 byte up to 127, 2 bytes up to 16383, max 5 bytes |
| `sint32`      | positions (`x`, `y`)         | zigzag varint: small negative values stay small            |
| `bool`        | flags                        | varint, 1 byte                                             |
| `string`      | chat text                    | length-prefixed, valid UTF-8                               |
| enum          | `RoomState`, `GameOverReason` | varint; unknown values are rejected by the receiver        |
| message       | nested records               | length-prefixed                                            |

- A field equal to its default (`0`, `false`, empty) is **not written** at all: a `LobbyCreate` costs 3 bytes (2-byte
  envelope tag for field 34, 1-byte length 0).
- Fields may appear in any order and unknown fields are skipped, so a peer may add fields without breaking older ones.
- Protobuf has no 8-bit integers: fields documented with a smaller range (0–255) are carried as `uint32` and
  range-checked on reception. `float` and `fixed32` always take 4 bytes.

---

## 3. Handshake

Before any of these messages, the network library runs its own handshake: its first frame carries a 32-bit `protocolId`
that identifies the game (R-Type: `0x50595452`, `TRANSPORT_PROTOCOL_ID`), and a peer announcing another one is refused
before the game sees it. See [Transport & Connection](transport.md#3-framing-and-validation).

### `Hello` (16)

**Direction:** Client → Server · **Channel:** Reliable · **Phase:** Handshake

Sent right after the library delivers its own `Welcome` (`PeerId` + token). Starts version negotiation.

| Field     | # | Type     | Description                                             |
|-----------|---|----------|---------------------------------------------------------|
| `version` | 1 | `uint32` | Client protocol version, MUST be `PROTOCOL_VERSION` (3) |

On a version mismatch, the server does not send `Welcome`: the transport tells the client why with a `Refused` frame
(see [transport.md](transport.md)), then closes the connection. The client MUST send `Hello`
within 5 seconds of connecting: past that, the server closes the connection (messages other than `Hello` do not reset
the timer).

### `Welcome` (17)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** Handshake

| Field      | # | Type     | Description                                       |
|------------|---|----------|---------------------------------------------------|
| `udp_port` | 1 | `uint32` | Server UDP port (1–65535), same as TCP by default |

The client keeps this port, and calls `openUdp(udp_port)` on the network library at `GameStart`.

---

## 4. Lobby

### `LobbyListRequest` (32)

**Direction:** Client → Server · **Channel:** Reliable · **Phase:** Lobby

No field. Asks for a `LobbyList`.

### `LobbyList` (33)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** Lobby

Answer to `LobbyListRequest`, also pushed to every client in the lobby phase whenever a room is created, destroyed, or
changes its number of players or its state.

| Field   | # | Type                 | Description                     |
|---------|---|----------------------|---------------------------------|
| `rooms` | 1 | repeated `RoomEntry` | At most 256 rooms (`MAX_ROOMS`) |

**`RoomEntry`**

| Field          | # | Type        | Description                                        |
|----------------|---|-------------|----------------------------------------------------|
| `room_id`      | 1 | `uint32`    | Room id, assigned by the server                    |
| `player_count` | 2 | `uint32`    | Current number of players (0 to `max_players`)     |
| `max_players`  | 3 | `uint32`    | Capacity of the room (1–4)                         |
| `state`        | 4 | `RoomState` | `ROOM_STATE_WAITING` (0), `ROOM_STATE_IN_GAME` (1) |

### `LobbyCreate` (34)

**Direction:** Client → Server · **Channel:** Reliable · **Phase:** Lobby

No field. Creates a room and joins it; the server then sends a `LobbyUpdate` and broadcasts an updated `LobbyList`.
Ignored if the client is already in a room, or if the server already has 256 rooms (`MAX_ROOMS`).

### `LobbyJoin` (35)

**Direction:** Client → Server · **Channel:** Reliable · **Phase:** Lobby

| Field     | # | Type     | Description  |
|-----------|---|----------|--------------|
| `room_id` | 1 | `uint32` | Room to join |

Ignored if the client is already in a room, or if the room is unknown, full or already in game.

### `LobbyReady` (36)

**Direction:** Client → Server · **Channel:** Reliable · **Phase:** Lobby

| Field   | # | Type   | Description                |
|---------|---|--------|----------------------------|
| `ready` | 1 | `bool` | `true` when ready to start |

Ignored if the client is not in a room. The server starts the game when every player of the room is ready, which can
also happen when the last player who was not ready leaves.

### `LobbyUpdate` (37)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** Lobby

Pushed whenever a player joins, leaves, or changes their ready state.

| Field     | # | Type                  | Description                      |
|-----------|---|-----------------------|----------------------------------|
| `room_id` | 1 | `uint32`              | Updated room                     |
| `slots`   | 2 | repeated `PlayerSlot` | Players in join order, at most 4 |

**`PlayerSlot`**

| Field       | # | Type     | Description              |
|-------------|---|----------|--------------------------|
| `client_id` | 1 | `uint32` | Network id of the player |
| `ready`     | 2 | `bool`   | Ready state              |

Only occupied slots are sent: the number of players is `slots` size.

---

## 5. Game Events

### `GameStart` (48)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** Lobby → In-game

| Field          | # | Type     | Description                                            |
|----------------|---|----------|--------------------------------------------------------|
| `room_id`      | 1 | `uint32` | Room that started                                      |
| `player_index` | 2 | `uint32` | Ship controlled by this client, in `[0, player_count)` |
| `player_count` | 3 | `uint32` | Number of players in the game (1–4)                    |

The client then opens UDP on the port received in `Welcome` and switches to the in-game phase. `player_index` is the position of the player in join
order among those present at the start, and is stable for the whole game: it does not change when another player leaves.

### `GameOver` (49)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** In-game → Lobby

| Field           | # | Type              | Description                                          |
|-----------------|---|-------------------|------------------------------------------------------|
| `reason`        | 1 | `GameOverReason`  | `ALL_DEAD` (0), `VICTORY` (1), `SERVER_SHUTDOWN` (2) |
| `score`         | 2 | `uint32`          | Final cumulative score                               |
| `player_scores` | 3 | repeated `uint32` | Score per player slot, at most 4 (packed)            |

Sent after the last level (`VICTORY`), when every ship is destroyed (`ALL_DEAD`), or when the server stops or a room
fails (`SERVER_SHUTDOWN`). The server then destroys the room. The client closes UDP and goes back to the lobby, where
it receives the list of rooms again.

### `LevelStart` (51)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** In-game

| Field         | # | Type     | Description                               |
|---------------|---|----------|-------------------------------------------|
| `level_index` | 1 | `uint32` | Index of the level, below `level_count`   |
| `level_count` | 2 | `uint32` | Number of levels of the game (1–64)       |
| `name`        | 3 | `string` | Display name, UTF-8, 0–64 bytes           |

The first one follows `GameStart`. The next ones follow a `LevelComplete`, after an intermission (3 s) during which
nothing spawns. The client shows the level name.

### `LevelComplete` (52)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** In-game

| Field           | # | Type              | Description                                |
|-----------------|---|-------------------|--------------------------------------------|
| `level_index`   | 1 | `uint32`          | Index of the cleared level (below 64)      |
| `score`         | 2 | `uint32`          | Cumulative score of the room so far        |
| `player_scores` | 3 | repeated `uint32` | Score per player slot, at most 4 (packed)  |

When a level is cleared, the server destroys every entity except the ships, moves the ships back to their start
positions, and sends this message. The next `LevelStart`, or `GameOver` after the last level, follows.

### `ScoreUpdate` (53)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** In-game

| Field           | # | Type              | Description                               |
|-----------------|---|-------------------|-------------------------------------------|
| `score`         | 1 | `uint32`          | Total score of the room                   |
| `player_scores` | 2 | repeated `uint32` | Score per player slot, at most 4 (packed) |

Sent when a score changes, at most every 0.25 s: a burst of kills is sent once. It travels on the reliable channel
like the other session messages: it is small and rare, and a lost update would leave a wrong score on screen until
the next kill. The client shows the total and its own score (`player_scores[GameStart.player_index]`).

### `PlayerLeft` (50)

**Direction:** Server → Client · **Channel:** Reliable · **Phase:** Lobby + In-game

| Field       | # | Type     | Description                       |
|-------------|---|----------|-----------------------------------|
| `client_id` | 1 | `uint32` | Network id of the player who left |

Sent to the remaining players of a room when one of them leaves or disconnects.

---

## 6. General

### `Leave` (64)

**Direction:** Client → Server · **Channel:** Reliable · **Phase:** Any

No field. Leaves the current room, waiting or in game; the client stays connected and is back in the lobby (it is
how a player goes back to the lobby from a game). Also sent before closing TCP, for a graceful disconnection.

### `Chat` (65)

**Direction:** Both · **Channel:** Reliable · **Phase:** Lobby + In-game

| Field       | # | Type     | Description                                                |
|-------------|---|----------|------------------------------------------------------------|
| `client_id` | 1 | `uint32` | Sender; ignored on reception by the server, which fills it |
| `text`      | 2 | `string` | UTF-8 text, 1–256 bytes                                    |

The server broadcasts a received `Chat` to the other players of the sender's room, with `client_id` set.

---

## 7. In-game (UDP)

UDP payloads MUST NOT exceed **1192 bytes** (1200-byte datagram minus the 8-byte library header).

### `PlayerInput` (1)

**Direction:** Client → Server · **Channel:** Unreliable · **Phase:** In-game

Sent every client tick (~60 Hz). A lost datagram is replaced by the next one.

| Field      | # | Type                 | Description                                                    |
|------------|---|----------------------|----------------------------------------------------------------|
| `sequence` | 1 | `uint32`             | Increases with every input (wrapping)                          |
| —          | 2 | —                    | Reserved: v1 `buttons` bitmask, never reuse                    |
| `held`     | 3 | repeated `fixed32`   | Action ids of the buttons held down, at most 16                |
| `axes`     | 4 | repeated `AxisValue` | Axis actions whose value is not 0, at most 8                   |

**`AxisValue`**

| Field    | # | Type      | Description            |
|----------|---|-----------|------------------------|
| `action` | 1 | `fixed32` | Action id              |
| `value`  | 2 | `float`   | In [-1, 1], finite     |

The protocol carries **actions, not keys**. An action id is the FNV-1a hash of the action's name, the same value as
the engine's `input::ActionId` (`input::makeActionId("shoot")`). The game declares its actions in its `ActionMapping`
(R-Type: `shoot`, `move_x`, `move_y`, see `client/src/input/RTypeBindings.cpp`), so another game sends its own
actions without any protocol change, and rebinding keys or using a gamepad changes nothing on the wire. Ids are
`fixed32` because a hash is uniformly spread: as a varint it would take 5 bytes most of the time.

Sizes: **4 bytes** when idle, **10** with one button held, **34** for R-Type's busiest input (shooting while moving
diagonally: 1 button + 2 axes), about 2 KB/s per player at 60 Hz. Inputs already applied or older than the last applied
one are dropped by the server.

### `WorldState` (2)

**Direction:** Server → Client · **Channel:** Unreliable · **Phase:** In-game

Sent after each server step. The client renders the most recent snapshot; parts of older ones (by `tick`, with wrapping)
MUST be dropped.

A snapshot too large for one datagram is split into several `WorldState`, the **parts** of the snapshot, each one holding
at most `MAX_ENTITIES_PER_PART` (36) entities, with the same `tick` and `last_processed_input`. A client applies each part
when it arrives, and ignores a part it already has. It MUST NOT remove an entity absent from a snapshot before it has
received all its parts: a lost part may have carried that entity. A world with no entity is still sent, as one empty part.
A snapshot needing more than `MAX_SNAPSHOT_PARTS` (64) parts, about 2300 entities, is not sent.

| Field                  | # | Type                   | Description                                    |
|------------------------|---|------------------------|------------------------------------------------|
| `tick`                 | 1 | `uint32`               | Server step of the snapshot (wrapping)         |
| `last_processed_input` | 2 | `uint32`               | `sequence` of this client's last applied input |
| `entities`             | 3 | repeated `EntityState` | Entities of this part of the snapshot          |
| `part_index`           | 4 | `uint32`               | Index of this part, below `part_count`         |
| `part_count`           | 5 | `uint32`               | Number of parts; `0` (not sent) means 1        |

`last_processed_input` lets the client reconcile its prediction: it snaps to the server state and replays the inputs
the server has not applied yet.

**`EntityState`**

| Field       | #  | Type      | Description                                               |
|-------------|----|-----------|-----------------------------------------------------------|
| `entity_id` | 1  | `uint32`  | Stable for the entity's whole life                        |
| —           | 2  | —         | Reserved: v1 R-Type `EntityType` enum, never reuse        |
| `subtype`   | 3  | `uint32`  | 0–255, game-defined variant of the kind                   |
| —           | 4  | —         | Reserved: v1 int16 pixel `x`, never reuse                 |
| —           | 5  | —         | Reserved: v1 int16 pixel `y`, never reuse                 |
| `health`    | 6  | `uint32`  | 0–255; 0 means the entity died this tick                  |
| `flags`     | 7  | `uint32`  | 0–255; game-defined bits                                  |
| `x`         | 8  | `float`   | X position in world units, same as `Transform::x`; finite |
| `y`         | 9  | `float`   | Y position in world units, same as `Transform::y`; finite |
| `kind`      | 10 | `fixed32` | FNV-1a hash of the entity's prefab name                   |

The protocol knows **no entity kind**: like input actions, kinds are the game's own. `kind` is the 32-bit FNV-1a hash
of the name of the prefab the entity was spawned from (`protocol::makeEntityKind("alien_scout")`, the same algorithm
as `input::makeActionId()`). The client maps each kind to its sprite or prefab; `subtype` and `flags` carry whatever the
game needs, see [Appendix A](#appendix-a--r-type-entity-conventions) for R-Type. A kind the client does not know is
not an error: it is up to the game to skip or show a placeholder.

Positions are sent as the engine stores them (`Transform` is in `float` world units): no conversion, no lost
sub-pixel precision for the client's interpolation.

An entity with `health = 0` is sent exactly **once** (the tick it died), so that clients can play a death animation.

**Size:** a snapshot header costs about 8 bytes and an entity about 26 bytes for typical values (at most 32; zero
fields are omitted, `kind`, `x` and `y` always take 5 bytes each), so a datagram holds about 45 entities, and at least 36 in
the worst case. The sender MUST keep the
whole snapshot within 1192 bytes; the encoder does not split snapshots (the server logs and skips an oversized one).

---

## 8. Validation

Every received payload is untrusted. `protocol::decode()` (`common/src/Protocol.cpp`) returns nothing — and the
payload MUST be dropped silently — when:

- the payload is empty or larger than 64 KiB;
- protobuf parsing fails (truncated varint, length past the end, invalid wire type, invalid UTF-8 in `text`);
- the envelope carries no message, or a message this build does not know;
- an enum holds an unknown value (`RoomState`, `GameOverReason`);
- a float is NaN or infinite (`x`, `y`, an axis `value`), or an axis `value` is outside [-1, 1];
- a number is outside its documented range (`subtype`, `health`, `flags` > 255; `udp_port`
  of 0 or > 65535; `max_players` of 0 or > 4; a room `player_count` > `max_players`; `GameStart.player_count` of 0
  or > 4; `player_index` >= `player_count`);
- a list is longer than its maximum (`rooms` > 256; `slots`, `player_scores` > 4, in `GameOver`, `LevelComplete` and
  `ScoreUpdate`; `held` > 16; `axes` > 8);
- a level message is out of range (`level_count` of 0 or > 64, `level_index` >= `level_count` in `LevelStart`,
  `level_index` >= 64 in `LevelComplete`, `name` longer than 64 bytes);
- a chat `text` is empty or longer than 256 bytes.

Phase and channel checks (a message valid but received in the wrong phase or on the wrong channel) are done by the
game, after decoding. The server currently enforces:

- before a successful `Hello`, every other message (reliable or unreliable) is dropped;
- a second `Hello` from the same client is dropped (no second `Welcome`);
- a client that sends no valid `Hello` within 5 seconds of connecting is kicked;
- over UDP, only `PlayerInput` is accepted, and only from a client in a room that has started;
- during a game, lobby messages (`LobbyListRequest`, `LobbyCreate`, `LobbyJoin`, `LobbyReady`) are dropped;
- `LobbyCreate` and `LobbyJoin` are dropped from a client already in a room, and `LobbyReady` from a client in none.

`Chat` is not implemented yet: the server drops it.

---

## 9. Why Protocol Buffers

- **Binary and compact.** Varints make the common case small (4-byte idle inputs) without a hand-written bit packer.
- **Robust parsing.** The protobuf parser is fuzzed upstream; our code only adds range checks. No manual offsets, no
  out-of-bounds reads.
- **Evolution.** New fields and messages are backward compatible: older peers skip unknown fields. The fixed-layout
  format needed a version bump for any change.
- **One schema, many languages.** `rtype.proto` documents the protocol and can generate a compatible client in any
  language.
- **Isolation.** Only `common/` sees protobuf types; game code uses plain structs (`protocol/Messages.hpp`), so the
  format can change without touching the client or the server.

The cost is a dependency (`protobuf-cpp`, built by xmake) and bigger snapshots than the former fixed 12-byte entity
layout (about 26 bytes per entity with float positions and a prefab hash), which is acceptable within one 1192-byte datagram.

---

## Appendix A — R-Type Entity Conventions

How R-Type fills the game-defined fields of `EntityState`. Another game defines its own.

| Field     | R-Type use                                                                                                                     |
|-----------|--------------------------------------------------------------------------------------------------------------------------------|
| `kind`    | `makeEntityKind(<prefab name>)`: `player_ship`, `alien_scout`, `alien_gunner`, `alien_elite`, `boss_core`, `boss_part`, `missile`, `obstacle`, `player_bullet`, `power_up` |
| `subtype` | Player index (0–3) of a player ship or a player bullet; height of an `obstacle` terrain block in 2 px units; variant of a `boss_core`; look of a `boss_part`; shot look of a `missile` (1: alien gunner, 2: alien elite, 0: others); 0 otherwise |
| `health`  | Lives left of a `player_ship` (3 at the start of a game, 0 once eliminated); hit points left of a `boss_core`, a turret `boss_part` or an `alien_elite` (1–255), 1 for the entities that die on the first hit; 0 the tick an entity dies |
| `flags`   | Bit 0: alive. Bit 1: shooting. Bit 2: invulnerable (a ship that just respawned; it blinks). Bit 3: eliminated (a ship that lost its last life; its player watches until the end of the game). Bits 4–7: reserved, 0 |

---

## Appendix B — Message Summary

| #    | Message            | Dir   | Channel    | Typical size (bytes)       |
|------|--------------------|-------|------------|----------------------------|
| `1`  | `PlayerInput`      | C → S | Unreliable | 4–34                       |
| `2`  | `WorldState`       | S → C | Unreliable | ~8 + 26 per entity (≤1192, split beyond) |
| `16` | `Hello`            | C → S | Reliable   | 5                          |
| `17` | `Welcome`          | S → C | Reliable   | 6                          |
| `32` | `LobbyListRequest` | C → S | Reliable   | 3                          |
| `33` | `LobbyList`        | S → C | Reliable   | 3 + ~10 per room           |
| `34` | `LobbyCreate`      | C → S | Reliable   | 3                          |
| `35` | `LobbyJoin`        | C → S | Reliable   | ~5                         |
| `36` | `LobbyReady`       | C → S | Reliable   | 3–5                        |
| `37` | `LobbyUpdate`      | S → C | Reliable   | ~5 + 6 per player          |
| `48` | `GameStart`        | S → C | Reliable   | ~9                         |
| `49` | `GameOver`         | S → C | Reliable   | ~14                        |
| `50` | `PlayerLeft`       | S → C | Reliable   | ~5                         |
| `51` | `LevelStart`       | S → C | Reliable   | ~7 + name length           |
| `52` | `LevelComplete`    | S → C | Reliable   | ~10                        |
| `53` | `ScoreUpdate`      | S → C | Reliable   | ~8                         |
| `64` | `Leave`            | C → S | Reliable   | 3                          |
| `65` | `Chat`             | Both  | Reliable   | ~7 + text length           |
