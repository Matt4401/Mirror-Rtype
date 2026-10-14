# Network: Library & Architecture Choices

This document compares the networking library and the design decisions of the network layer with their main
alternatives.

The resulting design is described in [Network Architecture](../architecture/network.md)
and [Network Transport & Connection](../protocol/transport.md).

---

## Table of contents

1. [Benchmark: why Asio](#1-benchmark-why-asio)
2. [Benchmark: why this architecture](#2-benchmark-why-this-architecture)

---

## 1. Benchmark: why Asio

Asio is a project requirement for the server. This section documents why it is also a sound technical choice compared to
the alternatives.

### Candidates

| Library                       | Type                                   |
|-------------------------------|----------------------------------------|
| Raw sockets (POSIX / Winsock) | OS API                                 |
| **Asio standalone**           | Generic async I/O, C++                 |
| Boost.Asio                    | Same library, distributed inside Boost |
| libuv                         | Generic async I/O, C                   |
| Qt Network                    | Generic networking, part of Qt         |
| ENet                          | Game-oriented reliable UDP, C          |
| GameNetworkingSockets (Valve) | Game-oriented transport, C++           |

### Comparison

| Criterion                         | Raw sockets   | **Asio**           | Boost.Asio    | libuv  | Qt Network      | ENet              | GNS               |
|-----------------------------------|---------------|--------------------|---------------|--------|-----------------|-------------------|-------------------|
| Linux + Windows with one codebase | ✗ (two APIs) | ✓                 | ✓            | ✓     | ✓              | ✓                | ✓                |
| TCP and UDP                       | ✓            | ✓                 | ✓            | ✓     | ✓              | UDP only          | UDP only          |
| Async event loop                  | ✗ (manual)   | ✓                 | ✓            | ✓     | ✓              | Polling           | Polling           |
| Timers, signals, DNS resolver     | ✗            | ✓                 | ✓            | ✓     | ✓              | ✗                | ✗                |
| Native C++ API                    | ✗ (C)        | ✓                 | ✓            | ✗ (C) | ✓              | ✗ (C)            | ✓                |
| Dependency weight                 | None          | Light, header-only | Heavy (Boost) | Light  | Very heavy (Qt) | Light             | Medium            |
| Full control over the wire format | ✓            | ✓                 | ✓            | ✓     | ✓              | ✗ (own protocol) | ✗ (own protocol) |
| Built-in reliability over UDP     | ✗            | ✗                 | ✗            | ✗     | ✗              | ✓                | ✓                |
| Satisfies the "Asio" requirement  | ✗            | ✓                 | ✓            | ✗     | ✗              | ✗                | ✗                |

### Analysis

- **Raw sockets** give full control but require two implementations (POSIX and Winsock) and a hand-written event loop.
  All the portability and async work that Asio already provides would be redone.
- **libuv** and **Qt Network** are solid, but libuv is a C API with callbacks, and Qt brings a large framework for a
  single feature.
- **ENet** and **GameNetworkingSockets** offer reliability over UDP out of the box, but impose their own wire protocol.
  Our protocol is specified in our own RFC, so they would take over the part we must design.
- **Boost.Asio** is the same code as Asio, with the cost of a Boost dependency and no extra benefit for this project.

**Decision: Asio standalone.** It meets the requirement, runs on Linux and Windows with one codebase, handles TCP, UDP,
timers and signals in a single event loop, and leaves the protocol entirely under our control. The missing piece
(reliability over UDP) is not needed for gameplay data and is covered by TCP elsewhere.

### What Asio does not do

Asio does not provide the protocol, serialization, client identification over UDP, acks or reordering. These are
implemented by the network library and the protocol layer.

---

## 2. Benchmark: why this architecture

Each design decision was compared with its main alternatives.

### 2.1 Transport strategy

| Criterion                                   | TCP only                 | UDP only + custom reliability | **TCP + UDP**            |
|---------------------------------------------|--------------------------|-------------------------------|--------------------------|
| Gameplay latency                            | ✗ Head-of-line blocking | ✓                            | ✓                       |
| Stale data dropped instead of retransmitted | ✗                       | ✓                            | ✓                       |
| Reliable lobby / handshake                  | ✓ Built-in              | Must be implemented           | ✓ Built-in              |
| Implementation effort                       | Low                      | High                          | Medium                   |
| Single port / socket                        | ✓                       | ✓                            | ✗ Two                   |
| Ordering between all messages               | ✓                       | ✓                            | ✗ Not across transports |

**Decision: TCP + UDP.** UDP removes head-of-line blocking where latency matters; TCP provides reliability for free
where it does not. The lack of ordering across transports is handled by keeping every state-related message on UDP.

### 2.2 API shape

| Criterion                                       | `sendTcp()` / `sendUdp()` | **`send(data, Channel)`** |
|-------------------------------------------------|---------------------------|---------------------------|
| Game code expresses intent, not mechanism       | ✗                        | ✓                        |
| Transport can change without touching game code | ✗                        | ✓                        |
| Number of methods to maintain                   | Higher                    | Lower                     |

**Decision: channels.** The transport is an internal detail of `NetworkServer` / `NetworkClient`.

### 2.3 Interface hierarchy

| Criterion                                       | One `INetworkManager` with every method | **Common base + specific interfaces** | Two unrelated interfaces |
|-------------------------------------------------|-----------------------------------------|---------------------------------------|--------------------------|
| Server `send(id, …)` vs client `send(…)`        | ✗ Ignored parameters                   | ✓                                    | ✓                       |
| Shared operations (`poll`, `stop`) in one place | ✓                                      | ✓                                    | ✗                       |
| Each side exposes only what it can do           | ✗                                      | ✓                                    | ✓                       |

**Decision: `INetworkManager` + `INetworkServer` / `INetworkClient`.** Only operations with identical signature and
meaning are shared.

### 2.4 Delivery of incoming data

| Criterion                                      | Callbacks into game code           | **Event queue + `poll()`** | `io_context.poll()` in the game loop |
|------------------------------------------------|------------------------------------|----------------------------|--------------------------------------|
| Game logic runs on one thread                  | ✗ Callbacks on the network thread | ✓                         | ✓                                   |
| Network keeps receiving while the game is busy | ✓                                 | ✓                         | ✗                                   |
| Locks needed in game code                      | ✓ Many                            | ✗                         | ✗                                   |
| Asio hidden from game code                     | ✓                                 | ✓                         | ✗                                   |

**Decision: event queue polled once per tick.** Reception never stalls, and the simulation stays single-threaded.

### 2.5 Client identification

| Criterion                                   | IP only | IP + port | **`ClientId` + session token** |
|---------------------------------------------|---------|-----------|--------------------------------|
| Several players behind one NAT              | ✗      | ✓        | ✓                             |
| Links the TCP connection to the UDP address | ✗      | ✗        | ✓                             |
| Survives port change (NAT rebinding)        | ✗      | ✗        | ✓                             |
| Resists spoofed source addresses            | ✗      | ✗        | ✓                             |

**Decision: `ClientId` + token**, issued over TCP and checked on every UDP packet.

### 2.6 Library scope

| Criterion                                | Library handles game messages | **Library transports bytes** |
|------------------------------------------|-------------------------------|------------------------------|
| Library reusable for another protocol    | ✗                            | ✓                           |
| Protocol changes require library changes | ✓                            | ✗                           |
| Separation of concerns                   | Low                           | High                         |

**Decision: bytes only.** Serialization and message types belong to the protocol layer described in the RFC.

### 2.7 Code sharing between client and server implementations

| Criterion                                   | Inheritance from an implementation base | **Composition (`EventQueue`, network thread helpers)** |
|---------------------------------------------|-----------------------------------------|--------------------------------------------------------|
| Avoids multiple inheritance with interfaces | ✗                                      | ✓                                                     |
| Components testable in isolation            | ✗                                      | ✓                                                     |
| Flexibility to share only what is needed    | Low                                     | High                                                   |

**Decision: composition** for the code shared by `NetworkServer` and `NetworkClient`.

### Summary

| Decision       | Chosen                 | Main reason                                           |
|----------------|------------------------|-------------------------------------------------------|
| Library        | Asio standalone        | Requirement, portable, full control over the protocol |
| Transport      | TCP + UDP              | Low latency in game, free reliability outside         |
| API            | Channels               | Intent over mechanism, transport swappable            |
| Interfaces     | Common base + specific | No meaningless methods                                |
| Incoming data  | Queue + `poll()`       | Single-threaded simulation                            |
| Identification | `ClientId` + token     | Links TCP/UDP, robust to NAT and spoofing             |
| Scope          | Bytes only             | Protocol evolves independently                        |
| Code sharing   | Composition            | Simpler, testable                                     |
