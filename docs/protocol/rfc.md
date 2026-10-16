# R-Type Network Protocol — RFC

**Version:** 1  
**Status:** Draft  
**Authors:** R-Type Team

---

## Abstract

This document specifies the application-level binary protocol used between the R-Type game client
(`r-type_client`) and server (`r-type_server`). It defines the packet format, opcode table,
connection lifecycle, versioning rules, and error-handling policy.

A developer with no access to the source code should be able to implement a compatible client
or server by reading this document together with the transport specification:
[Network Transport & Connection](transport.md).

For byte-by-byte packet layouts, see [Packet Reference](packets.md).  
For the network library architecture, see [Network Architecture](../architecture/network.md).

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Conventions](#2-conventions)
3. [Protocol Architecture](#3-protocol-architecture)
4. [Protocol Versioning](#4-protocol-versioning)
5. [Connection Lifecycle](#5-connection-lifecycle)
6. [Opcode Table](#6-opcode-table)
7. [Error Handling](#7-error-handling)
8. [Security Considerations](#8-security-considerations)
9. [Port Assignment](#9-port-assignment)
10. [References](#10-references)

---

## 1. Introduction

R-Type is a networked, horizontal-scrolling shoot-'em-up supporting 1 to 4 simultaneous players.
The protocol is split into two layers:

- **Transport layer** — handled by the network library. Deals with TCP framing, UDP binding,
  keepalives and timeouts. Documented in [transport.md](transport.md).
- **Game layer** — this document. Carries application messages (lobby, inputs, world state, chat)
  on top of the transport layer.

The protocol is **entirely binary**. No human-readable encoding (JSON, XML, plain text) is used.
All integers are **little-endian** unless stated otherwise.

The client-server model is **authoritative server**: all game logic runs on the server. The client
sends inputs and renders the world state that the server broadcasts.

---

## 2. Conventions

### Notation

- `u8`, `u16`, `u32` — unsigned integers of 8, 16, 32 bits, little-endian.
- `i16` — signed 16-bit integer, little-endian, two's complement.
- `u8[N]` — fixed-size byte array of N bytes.
- `LE` — little-endian byte order.
- All offsets are zero-based and expressed in bytes.
- "Reserved" fields **must** be set to `0` by the sender and **must** be ignored by the receiver.

### Requirements Language

The key words **MUST**, **MUST NOT**, **SHOULD**, **MAY** are used as defined in
[RFC 2119](https://www.rfc-editor.org/rfc/rfc2119).

### Diagrams

Packet diagrams show fields from left (lowest offset) to right (highest offset).

```
 0               1               2               3
 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    magic[0]   |    magic[1]   |    version    |    opcode     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 3. Protocol Architecture

### 3.1 Two-layer design

```
+-----------------------+           +-----------------------+
|     Game Client       |           |     Game Server       |
|                       |           |                       |
|  Game layer (this doc)|           |  Game layer (this doc)|
|  +-----------------+  |           |  +-----------------+  |
|  | Packet builder  |  |           |  | Packet builder  |  |
|  +-----------------+  |           |  +-----------------+  |
|        |  ^           |           |        |  ^           |
|        v  |           |           |        v  |           |
|  +-----------------+  |           |  +-----------------+  |
|  | INetworkClient  |  |           |  | INetworkServer  |  |
|  +-----------------+  |           |  +-----------------+  |
+-----------+-----------+           +-----------+-----------+
            |                                   |
            |   TCP (Reliable)                  |
            +===================================>
            |   UDP (Unreliable)                |
            +===================================>
```

The network library is opaque to the game: it only exposes `send(bytes, channel)` and
`poll() -> events`. The game layer never touches sockets directly.

### 3.2 Channels

| Channel      | Transport | Use                                                        |
|--------------|-----------|------------------------------------------------------------|
| `Reliable`   | TCP       | Handshake, lobby, game events that must arrive in order    |
| `Unreliable` | UDP       | Player inputs and world state sent every game tick         |

See [transport.md §1](transport.md#1-transport-tcp-or-udp) for the full classification table.

### 3.3 Game phases

The protocol defines three successive phases per game session:

| Phase         | Description                                              |
|---------------|----------------------------------------------------------|
| **Handshake** | Version negotiation immediately after TCP connection     |
| **Lobby**     | Room creation, joining, and ready-state management       |
| **In-game**   | Real-time exchange of inputs and world state over UDP    |

Opcodes are only valid in the phases listed in the [Opcode Table](#6-opcode-table). A message
received in the wrong phase **MUST** be silently dropped.

---

## 4. Protocol Versioning

Every game message starts with a 4-byte header:

```
magic[0] = 0x52  ('R')
magic[1] = 0x54  ('T')
version  = 0x01
opcode   = <opcode>
```

The `magic` field allows quick validation. The `version` field is the protocol version.

**Version negotiation:**

1. The client sends `HELLO` with its `client_version` field set to `0x01`.
2. The server rejects any `HELLO` where `client_version != 0x01` by closing the TCP connection.
3. Once the server sends `WELCOME`, both sides agree on version `0x01` for the session.

Future versions increment the `version` byte. A server supporting version N **MAY** accept
clients announcing N-1 (backward compatibility), but this is not required.

---

## 5. Connection Lifecycle

The full connection lifecycle, including the UDP binding procedure, is described in
[transport.md §2](transport.md#2-connection-lifecycle).

From the game layer's perspective:

```
Client                                      Server
  |                                           |
  |-- TCP connect --------------------------> |  (transport layer)
  |<-- Welcome [ClientId, token] ----------- |  (transport layer, internal)
  |                                           |
  |-- HELLO (0x01) ------------------------> |  Handshake phase
  |<-- WELCOME (0x02) -------------------- - |
  |                                           |
  |-- LOBBY_LIST_REQ (0x10) --------------> |  Lobby phase
  |<-- LOBBY_LIST (0x11) ---------------- - |
  |-- LOBBY_CREATE (0x12)  OR               |
  |   LOBBY_JOIN (0x13) -----------------> |
  |<-- LOBBY_UPDATE (0x15) -------------- - |
  |-- LOBBY_READY (0x14) ----------------> |
  |<-- GAME_START (0x20) ---------------- - |
  |                                           |
  |-- [UDP open] --------------------------> |  (transport layer)
  |-- PLAYER_INPUT (0x60) [UDP] ----------> |  In-game phase
  |<-- WORLD_STATE (0x70) [UDP] --------- - |
  |                                           |
  |<-- GAME_OVER (0x30) [TCP] ----------- - |
  |-- [UDP close] -------------------------> |  (transport layer)
  |-- LEAVE (0x40) [TCP] ----------------> |
  |<-- TCP closed -------------------------- |
```

---

## 6. Opcode Table

| Opcode | Name              | Direction | Channel    | Phase             |
|--------|-------------------|-----------|------------|-------------------|
| `0x01` | `HELLO`           | C → S     | Reliable   | Handshake         |
| `0x02` | `WELCOME`         | S → C     | Reliable   | Handshake         |
| `0x10` | `LOBBY_LIST_REQ`  | C → S     | Reliable   | Lobby             |
| `0x11` | `LOBBY_LIST`      | S → C     | Reliable   | Lobby             |
| `0x12` | `LOBBY_CREATE`    | C → S     | Reliable   | Lobby             |
| `0x13` | `LOBBY_JOIN`      | C → S     | Reliable   | Lobby             |
| `0x14` | `LOBBY_READY`     | C → S     | Reliable   | Lobby             |
| `0x15` | `LOBBY_UPDATE`    | S → C     | Reliable   | Lobby             |
| `0x20` | `GAME_START`      | S → C     | Reliable   | Lobby → In-game   |
| `0x30` | `GAME_OVER`       | S → C     | Reliable   | In-game → Lobby   |
| `0x40` | `LEAVE`           | C → S     | Reliable   | Any               |
| `0x50` | `CHAT`            | Both      | Reliable   | Lobby + In-game   |
| `0x60` | `PLAYER_INPUT`    | C → S     | Unreliable | In-game           |
| `0x70` | `WORLD_STATE`     | S → C     | Unreliable | In-game           |

---

## 7. Error Handling

### 7.1 Malformed packets

A packet is considered malformed if any of the following conditions hold:

- Its total length is shorter than 4 bytes (the header).
- `magic[0] != 0x52` or `magic[1] != 0x54`.
- `version` does not match the negotiated version.
- The opcode is unknown.
- The payload is shorter than the minimum size required for the opcode.
- A field value is outside its defined range.

**Malformed packets MUST be silently dropped.** The receiver MUST NOT close the connection or
send an error reply. This prevents a misbehaving peer from forcing a disconnect.

### 7.2 Wrong phase

A valid packet received in the wrong phase (e.g. `PLAYER_INPUT` during the lobby) **MUST** be
silently dropped.

### 7.3 Transport-layer errors

The transport layer handles connection drops, UDP timeouts, and write-queue overflows autonomously.
When it delivers a `Disconnected` event, the game layer discards all state for that `ClientId`.
The game layer never needs to handle raw socket errors.

---

## 8. Security Considerations

### 8.1 Token-based UDP authentication

Every client-to-server UDP datagram is prefixed with `[ClientId u32][token u32]` by the transport
layer. The token is a 32-bit cryptographically random value generated per TCP session
(`std::random_device`). A datagram with an unknown `ClientId` or a mismatched token is dropped
before the game layer ever sees it.

### 8.2 Input validation

The game layer **MUST** validate every field before acting on it. In particular:

- `entity_id` values in `WORLD_STATE` that are not in the known entity table **MUST** be ignored.
- `player_index` in `GAME_START` outside `[0, player_count)` **MUST** cause the client to
  disconnect.
- `CHAT` messages longer than 256 bytes **MUST** be truncated or dropped.

### 8.3 Denial of service

- The server accepts at most **64 simultaneous TCP connections** (enforced by the transport layer).
- UDP datagrams larger than **1200 bytes** are dropped by the transport layer before reaching the
  game layer.
- A TCP write queue exceeding **1 MiB** per peer causes the session to be closed by the transport
  layer.

### 8.4 Authoritative server

The server is the sole authority on game state. Clients send only inputs (`PLAYER_INPUT`). Any
client that attempts to send game-state packets over UDP has those datagrams dropped because their
opcode does not match `0x60`.

---

## 9. Port Assignment

| Protocol | Default Port | Notes                          |
|----------|-------------|--------------------------------|
| TCP      | `4242`      | Client connects here first     |
| UDP      | `4242`      | Same port; server multiplexes  |

The server binds both TCP and UDP to the same port. The client learns the UDP port from the
`WELCOME` packet's `udp_port` field, which allows the operator to choose a different port at
startup.

---

## 10. References

- [Network Transport & Connection](transport.md) — TCP/UDP channel rules, connection lifecycle,
  framing
- [Packet Reference](packets.md) — binary layout of every opcode
- [Network Architecture](../architecture/network.md) — library design and threading model
- [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119) — Key words for use in RFCs
- [RFC 768](https://www.rfc-editor.org/rfc/rfc768) — User Datagram Protocol
- [RFC 793](https://www.rfc-editor.org/rfc/rfc793) — Transmission Control Protocol
