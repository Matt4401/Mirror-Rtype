# Packet Reference

This page provides the byte-by-byte binary layout of every game-layer packet.  
For the transport framing that wraps these payloads, see [transport.md](transport.md).  
For the protocol overview and connection lifecycle, see [rfc.md](rfc.md).

---

## Table of Contents

1. [Common Header](#1-common-header)
2. [Data Types](#2-data-types)
3. [TCP Messages — Handshake](#3-tcp-messages--handshake)
4. [TCP Messages — Lobby](#4-tcp-messages--lobby)
5. [TCP Messages — Game Events](#5-tcp-messages--game-events)
6. [TCP Messages — General](#6-tcp-messages--general)
7. [UDP Messages — In-game](#7-udp-messages--in-game)
8. [Appendix A — Entity Types](#appendix-a--entity-types)
9. [Appendix B — Button Bitmask](#appendix-b--button-bitmask)
10. [Appendix C — Complete Opcode Summary](#appendix-c--complete-opcode-summary)

---

## 1. Common Header

Every game-layer packet starts with a 4-byte header:

```
 Byte 0    Byte 1    Byte 2     Byte 3
+--------+--------+---------+----------+
|  0x52  |  0x54  | version |  opcode  |
+--------+--------+---------+----------+
  'R'      'T'       0x01    see table
```

| Field     | Type  | Value  | Description                    |
|-----------|-------|--------|--------------------------------|
| `magic`   | u8[2] | `RT`   | `0x52 0x54` — protocol magic   |
| `version` | u8    | `0x01` | Protocol version               |
| `opcode`  | u8    | varies | Message type — see opcode table |

A packet whose `magic` is not `0x52 0x54`, whose `version` does not match, or whose `opcode`
is unknown **MUST** be silently dropped.

---

## 2. Data Types

| Type     | Size    | Description                                     |
|----------|---------|-------------------------------------------------|
| `u8`     | 1 byte  | Unsigned 8-bit integer                          |
| `u16`    | 2 bytes | Unsigned 16-bit integer, little-endian          |
| `u32`    | 4 bytes | Unsigned 32-bit integer, little-endian          |
| `i16`    | 2 bytes | Signed 16-bit integer, little-endian (2's comp) |
| `u8[N]`  | N bytes | Fixed-size byte array                           |
| `bool`   | 1 byte  | `0x00` = false, `0x01` = true                   |
| `str256` | 2+N     | u16 length prefix (max 256), then UTF-8 bytes   |

> **Endianness** — all multi-byte integers are little-endian throughout this protocol.

---

## 3. TCP Messages — Handshake

### 0x01 — HELLO

**Direction:** Client → Server  
**Channel:** Reliable (TCP)  
**Phase:** Handshake  
**Size:** 5 bytes

Sent immediately after the transport layer delivers the internal `Welcome` message (i.e. after
`ClientId` and `token` are received). Starts version negotiation.

```
Offset  Size  Type  Field            Description
------  ----  ----  ---------------  --------------------------------------------------
0       2     u8[2] magic            0x52 0x54
2       1     u8    version          0x01
3       1     u8    opcode           0x01
4       1     u8    client_version   Client's protocol version — MUST be 0x01
```

If `client_version` does not match the server's version, the server closes the TCP connection
without sending `WELCOME`.

---

### 0x02 — WELCOME

**Direction:** Server → Client  
**Channel:** Reliable (TCP)  
**Phase:** Handshake  
**Size:** 6 bytes

Reply to `HELLO`. Confirms the handshake and provides the UDP port the client must use.

```
Offset  Size  Type  Field         Description
------  ----  ----  ------------  --------------------------------------------------
0       2     u8[2] magic         0x52 0x54
2       1     u8    version       0x01
3       1     u8    opcode        0x02
4       2     u16   udp_port      Server's UDP listening port (same as TCP by default)
```

After receiving `WELCOME`, the client calls `openUdp(udp_port)` on the transport layer.

---

## 4. TCP Messages — Lobby

### 0x10 — LOBBY_LIST_REQ

**Direction:** Client → Server  
**Channel:** Reliable (TCP)  
**Phase:** Lobby  
**Size:** 4 bytes

Requests the current list of available rooms.

```
Offset  Size  Type  Field    Description
------  ----  ----  -------  --------------------------------------------------
0       2     u8[2] magic    0x52 0x54
2       1     u8    version  0x01
3       1     u8    opcode   0x10
```

---

### 0x11 — LOBBY_LIST

**Direction:** Server → Client  
**Channel:** Reliable (TCP)  
**Phase:** Lobby  
**Size:** 5 + count × 8 bytes

Delivers the list of rooms. Sent in response to `LOBBY_LIST_REQ` and also pushed proactively
whenever a room is created, modified, or destroyed.

```
Offset  Size          Type      Field    Description
------  ------------  --------  -------  --------------------------------------------------
0       2             u8[2]     magic    0x52 0x54
2       1             u8        version  0x01
3       1             u8        opcode   0x11
4       1             u8        count    Number of RoomEntry records (0–8)
5       count × 8     RoomEntry rooms[]  One entry per room — see below
```

**RoomEntry (8 bytes):**

```
Offset  Size  Type  Field           Description
------  ----  ----  --------------  --------------------------------------------------
0       4     u32   room_id         Unique room identifier, assigned by the server
4       1     u8    player_count    Current number of players (0–4)
5       1     u8    max_players     Always 4
6       1     u8    state           0 = Waiting, 1 = InGame
7       1     u8    reserved        Set to 0
```

---

### 0x12 — LOBBY_CREATE

**Direction:** Client → Server  
**Channel:** Reliable (TCP)  
**Phase:** Lobby  
**Size:** 4 bytes

Requests the creation of a new room. The server assigns a `room_id` and broadcasts an updated
`LOBBY_LIST` to all clients.

```
Offset  Size  Type  Field    Description
------  ----  ----  -------  --------------------------------------------------
0       2     u8[2] magic    0x52 0x54
2       1     u8    version  0x01
3       1     u8    opcode   0x12
```

---

### 0x13 — LOBBY_JOIN

**Direction:** Client → Server  
**Channel:** Reliable (TCP)  
**Phase:** Lobby  
**Size:** 8 bytes

Requests to join an existing room. Ignored if the room is full (`player_count == 4`) or in
state `InGame`.

```
Offset  Size  Type  Field    Description
------  ----  ----  -------  --------------------------------------------------
0       2     u8[2] magic    0x52 0x54
2       1     u8    version  0x01
3       1     u8    opcode   0x13
4       4     u32   room_id  ID of the room to join
```

---

### 0x14 — LOBBY_READY

**Direction:** Client → Server  
**Channel:** Reliable (TCP)  
**Phase:** Lobby  
**Size:** 5 bytes

Toggles the client's ready state. The server starts the game when **all players** in the room
have `ready = 1` and `player_count >= 1`.

```
Offset  Size  Type  Field    Description
------  ----  ----  -------  --------------------------------------------------
0       2     u8[2] magic    0x52 0x54
2       1     u8    version  0x01
3       1     u8    opcode   0x14
4       1     bool  ready    0 = Not ready, 1 = Ready
```

---

### 0x15 — LOBBY_UPDATE

**Direction:** Server → Client  
**Channel:** Reliable (TCP)  
**Phase:** Lobby  
**Size:** 9 + 4 × 5 = 29 bytes

Pushed whenever a player joins, leaves, or changes their ready state in a room.

```
Offset  Size       Type        Field          Description
------  ---------  ----------  -------------  --------------------------------------------------
0       2          u8[2]       magic          0x52 0x54
2       1          u8          version        0x01
3       1          u8          opcode         0x15
4       4          u32         room_id        ID of the updated room
8       1          u8          player_count   Current number of players (0–4)
9       4 × 5      PlayerSlot  slots[]        4 fixed slots — see below
```

**PlayerSlot (5 bytes):**

```
Offset  Size  Type  Field      Description
------  ----  ----  ---------  --------------------------------------------------
0       4     u32   client_id  0 = empty slot; otherwise the client's ClientId
4       1     bool  ready      0 = Not ready, 1 = Ready
```

Slots are ordered by join order. Empty slots have `client_id = 0` and `ready = 0`.

---

## 5. TCP Messages — Game Events

### 0x20 — GAME_START

**Direction:** Server → Client  
**Channel:** Reliable (TCP)  
**Phase:** Lobby → In-game  
**Size:** 10 bytes

Sent to every client in the room when all players are ready. After receiving this message, the
client opens the UDP channel and switches to the In-game phase.

```
Offset  Size  Type  Field          Description
------  ----  ----  -------------  --------------------------------------------------
0       2     u8[2] magic          0x52 0x54
2       1     u8    version        0x01
3       1     u8    opcode         0x20
4       4     u32   room_id        ID of the room that started
8       1     u8    player_index   This client's slot index in the game (0–3)
9       1     u8    player_count   Total number of players in the game (1–4)
```

`player_index` identifies which spaceship this client controls. It is stable for the duration
of the game.

---

### 0x30 — GAME_OVER

**Direction:** Server → Client  
**Channel:** Reliable (TCP)  
**Phase:** In-game → Lobby  
**Size:** 25 bytes

Sent to all clients when the game ends. After receiving this message, the client closes the UDP
channel and returns to the Lobby phase.

```
Offset  Size   Type    Field           Description
------  -----  ------  --------------  --------------------------------------------------
0       2      u8[2]   magic           0x52 0x54
2       1      u8      version         0x01
3       1      u8      opcode          0x30
4       1      u8      reason          0 = AllDead, 1 = Victory, 2 = ServerShutdown
5       4      u32     score           Final cumulative score
9       4 × 4  u32[4]  player_scores   Score per player slot (0 if slot was empty)
```

---

## 6. TCP Messages — General

### 0x40 — LEAVE

**Direction:** Client → Server  
**Channel:** Reliable (TCP)  
**Phase:** Any  
**Size:** 4 bytes

Graceful disconnect. The client sends this before closing the TCP connection. The server
removes the client from its room (if any) and pushes an updated `LOBBY_LIST` to all remaining
clients.

```
Offset  Size  Type  Field    Description
------  ----  ----  -------  --------------------------------------------------
0       2     u8[2] magic    0x52 0x54
2       1     u8    version  0x01
3       1     u8    opcode   0x40
```

---

### 0x50 — CHAT

**Direction:** Client → Server and Server → Client  
**Channel:** Reliable (TCP)  
**Phase:** Lobby + In-game  
**Size:** 10 + message_length bytes (max 266 bytes)

Chat message. When the server receives a `CHAT` from a client, it broadcasts it to all other
clients in the same room with the sender's `client_id` filled in.

```
Offset  Size  Type   Field          Description
------  ----  -----  -------------  --------------------------------------------------
0       2     u8[2]  magic          0x52 0x54
2       1     u8     version        0x01
3       1     u8     opcode         0x50
4       4     u32    client_id      Sender's ClientId (filled by the server on broadcast)
8       2     u16    length         Byte length of the message (1–256)
10      N     u8[N]  message        UTF-8 text, not null-terminated
```

Messages longer than **256 bytes** MUST be truncated or dropped by the server.

---

## 7. UDP Messages — In-game

UDP game payloads are delivered by the transport layer as raw bytes with no additional framing
(the transport `ClientId`/`token` header is stripped before the game layer receives the data).
The common 4-byte header still applies.

> **Size limit:** UDP game payloads MUST NOT exceed **1192 bytes** (1200 byte datagram minus
> 8 bytes of transport header for client→server direction).

### 0x60 — PLAYER_INPUT

**Direction:** Client → Server  
**Channel:** Unreliable (UDP)  
**Phase:** In-game  
**Size:** 9 bytes

Sent every game tick (~60 Hz) to report the current state of the player's controls. Because
inputs are sent continuously, a lost datagram is simply replaced by the next one. The server
applies the input from the most recent `seq` it has received for each client.

```
Offset  Size  Type  Field    Description
------  ----  ----  -------  --------------------------------------------------
0       2     u8[2] magic    0x52 0x54
2       1     u8    version  0x01
3       1     u8    opcode   0x60
4       4     u32   seq      Monotonically increasing tick counter (wrapping u32)
8       1     u8    buttons  Bitmask — see Appendix B
```

Stale inputs (seq <= last received seq, accounting for wrapping) MUST be discarded.

---

### 0x70 — WORLD_STATE

**Direction:** Server → Client  
**Channel:** Unreliable (UDP)  
**Phase:** In-game  
**Size:** 9 + entity_count × 12 bytes (max: 9 + 98 × 12 = 1185 bytes)

Sent every game tick (~60 Hz). Contains the authoritative snapshot of all entities in the
world. Clients should render the most recent snapshot they have received; older snapshots
MUST be discarded (`tick` comparison with wrapping).

```
Offset              Size   Type         Field          Description
------              -----  -----------  -------------  --------------------------------------------------
0                   2      u8[2]        magic          0x52 0x54
2                   1      u8           version        0x01
3                   1      u8           opcode         0x70
4                   4      u32          tick           Server tick number (wrapping u32, ~60 per second)
8                   1      u8           entity_count   Number of EntityState records (0–98)
9                   N×12   EntityState  entities[]     One entry per entity — see below
```

**EntityState (12 bytes):**

```
Offset  Size  Type  Field      Description
------  ----  ----  ---------  --------------------------------------------------
0       4     u32   entity_id  Unique entity identifier, stable for the entity's lifetime
4       1     u8    type       Entity type — see Appendix A
5       1     u8    subtype    Type-specific variant (player index, enemy model, etc.)
6       2     i16   x          X position in pixels (0 = left edge)
8       2     i16   y          Y position in pixels (0 = top edge)
10      1     u8    health     Current health points. 0 means the entity died this tick
                               and will not appear in subsequent snapshots.
11      1     u8    flags      Bitmask: bit 0 = alive, bit 1 = shooting, bits 2–7 reserved
```

An entity with `health = 0` is included in the snapshot exactly **once** (the tick it died)
to allow clients to play a death animation. It MUST NOT appear in subsequent snapshots.

---

## Appendix A — Entity Types

| Value | Name            | Description                                      |
|-------|-----------------|--------------------------------------------------|
| `0`   | `Player`        | Player-controlled spaceship. `subtype` = player index (0–3) |
| `1`   | `Enemy`         | Bydos enemy unit. `subtype` = enemy model ID     |
| `2`   | `PlayerMissile` | Missile fired by a player. `subtype` = player index |
| `3`   | `EnemyMissile`  | Missile fired by an enemy. `subtype` = 0         |
| `4`   | `Powerup`       | Powerup collectible. `subtype` = powerup type    |

---

## Appendix B — Button Bitmask

Used in the `buttons` field of `PLAYER_INPUT`.

| Bit | Mask   | Name    | Description                   |
|-----|--------|---------|-------------------------------|
| 0   | `0x01` | `UP`    | Move up (arrow key ↑)         |
| 1   | `0x02` | `DOWN`  | Move down (arrow key ↓)       |
| 2   | `0x04` | `LEFT`  | Move left (arrow key ←)       |
| 3   | `0x08` | `RIGHT` | Move right (arrow key →)      |
| 4   | `0x10` | `SHOOT` | Fire missile (Space bar)      |
| 5–7 | `0xE0` | —       | Reserved — MUST be set to `0` |

Multiple bits can be set simultaneously (e.g. `0x09` = UP + RIGHT).

---

## Appendix C — Complete Opcode Summary

| Opcode | Name             | Dir   | Chan       | Size (bytes)                   |
|--------|------------------|-------|------------|-------------------------------|
| `0x01` | `HELLO`          | C → S | Reliable   | 5                             |
| `0x02` | `WELCOME`        | S → C | Reliable   | 6                             |
| `0x10` | `LOBBY_LIST_REQ` | C → S | Reliable   | 4                             |
| `0x11` | `LOBBY_LIST`     | S → C | Reliable   | 5 + count × 8                 |
| `0x12` | `LOBBY_CREATE`   | C → S | Reliable   | 4                             |
| `0x13` | `LOBBY_JOIN`     | C → S | Reliable   | 8                             |
| `0x14` | `LOBBY_READY`    | C → S | Reliable   | 5                             |
| `0x15` | `LOBBY_UPDATE`   | S → C | Reliable   | 29                            |
| `0x20` | `GAME_START`     | S → C | Reliable   | 10                            |
| `0x30` | `GAME_OVER`      | S → C | Reliable   | 25                            |
| `0x40` | `LEAVE`          | C → S | Reliable   | 4                             |
| `0x50` | `CHAT`           | Both  | Reliable   | 10 + length (max 266)         |
| `0x60` | `PLAYER_INPUT`   | C → S | Unreliable | 9                             |
| `0x70` | `WORLD_STATE`    | S → C | Unreliable | 9 + entity_count × 12 (≤1185) |
