# Network Architecture — Developer Guide

This document explains how the networking layer is designed, why it is designed this way, and how the server and the
client use it.

It covers the **architecture** of the network library only. Related pages:

- [Network Transport & Connection](../protocol/transport.md): which messages use which channel, connection lifecycle,
  framing and validation.
- [Protocol RFC](../protocol/rfc.md) and [Packets](../protocol/packets.md): byte layout of each packet.
- [Network comparative study](../comparative-study/network.md): why Asio and why this architecture.

---

## Table of contents

1. [Requirements](#1-requirements)
2. [Overview](#2-overview)
3. [Library layout](#3-library-layout)
4. [Public API](#4-public-api)
5. [Internal design](#5-internal-design)
6. [Threading model](#6-threading-model)
7. [Using the library](#7-using-the-library)
8. [Pitfalls](#8-pitfalls)

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
2. **Interfaces only.** Game code never sees Asio, sockets or endpoints, only `ClientId`s and bytes.
3. **Polling.** The library receives on its own thread and queues events; game code pulls them once per tick.

---

## 3. Library layout

```
network/
├── include/network/            # PUBLIC — must not include Asio
│   ├── Types.hpp
│   ├── INetworkManager.hpp
│   ├── INetworkServer.hpp
│   ├── INetworkClient.hpp
│   └── Factory.hpp
└── src/                        # PRIVATE — Asio is used only here
    ├── NetworkServer.hpp/.cpp
    ├── NetworkClient.hpp/.cpp
    ├── TcpServer.hpp/.cpp
    ├── session/TcpSession.hpp/.cpp     # shared by NetworkServer and NetworkClient
    ├── UdpServer.hpp/.cpp
    ├── EventQueue.hpp/.cpp
    └── Factory.cpp
```

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

```cpp
namespace net {

using ClientId = uint32_t;
using Bytes    = std::vector<uint8_t>;

enum class Channel   { Reliable, Unreliable };
enum class EventType { Connected, Disconnected, Data };

struct NetworkEvent {
    EventType type;
    ClientId  client;    // always 0 on the client side
    Channel   channel;   // meaningful for Data events
    Bytes     data;      // empty for Connected / Disconnected
};

}
```

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
    virtual void send(ClientId client, const Bytes& data, Channel channel) = 0;
    virtual void broadcast(const Bytes& data, Channel channel) = 0;
    virtual void kick(ClientId client) = 0;
};

class INetworkClient : public INetworkManager {
public:
    virtual bool connect(const std::string& host, uint16_t port) = 0;
    virtual bool openUdp(uint16_t port) = 0;
    virtual void closeUdp() = 0;
    virtual void send(const Bytes& data, Channel channel) = 0;
};
```

```
          INetworkManager          poll · stop · isRunning
           /            \
  INetworkServer      INetworkClient
  start · send(id)    connect · openUdp
  broadcast · kick    closeUdp · send
           |                 |
    NetworkServer      NetworkClient       (private)
```

### Factory

```cpp
std::unique_ptr<INetworkServer> net::createServer();
std::unique_ptr<INetworkClient> net::createClient();
```

Game code never includes an implementation header, so it never includes Asio.

---

## 5. Internal design

### Server

```
NetworkServer                         implements INetworkServer
├── asio::io_context + std::thread    single network thread (TCP and UDP)
├── EventQueue                        shared by TcpServer and UdpServer
├── TcpServer
│   ├── tcp::acceptor
│   └── map<ClientId, shared_ptr<TcpSession>>
└── UdpServer
    ├── udp::socket                   one socket for all clients
    └── map<udp::endpoint, ClientId>
```

| Class           | Responsibility                                                                                                                                                        |
|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `NetworkServer` | Owns the `io_context`, the network thread and the event queue. Routes `send()` to TCP or UDP according to the channel.                                                |
| `TcpServer`     | Accepts connections, assigns `ClientId` and session token, stores sessions.                                                                                           |
| `TcpSession`    | One TCP connection: length-prefixed reads, writes, close. Inherits `enable_shared_from_this` so pending async operations keep it alive. Also used by `NetworkClient`. |
| `UdpServer`     | Receives all datagrams, validates tokens, maps endpoints to `ClientId`, tracks `lastSeen` for timeouts.                                                               |
| `EventQueue`    | Mutex-protected queue. The network thread pushes; `poll()` pops everything at once.                                                                                   |

### Routing

```cpp
void NetworkServer::send(ClientId client, const Bytes& data, Channel channel) {
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
only once, so client and server cannot drift apart. The client's session always has `ClientId` 0, and its close
handler pushes a `Disconnected` event.

| Function        | Effect                                                                                                    |
|-----------------|-----------------------------------------------------------------------------------------------------------|
| `connect()`     | Opens TCP only, creates the `TcpSession`, starts the network thread.                                      |
| `openUdp(port)` | Called at `GAME_START` with the UDP port from `WELCOME`. Targets the address of the TCP server on `port`. |
| `closeUdp()`    | Called at `GAME_OVER`. The client goes back to TCP only (lobby).                                          |

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
| Only the game thread touches game state     | The simulation stays single-threaded, no locks in game code   |
| `poll()` returns all pending events at once | One lock per tick instead of one per packet                   |

---

## 7. Using the library

### Server

```cpp
class GameServer {
public:
    explicit GameServer(net::INetworkServer& network) : network_(network) {}

    void tick() {
        for (auto& e : network_.poll()) {
            switch (e.type) {
                case net::EventType::Connected:    onJoin(e.client);  break;
                case net::EventType::Disconnected: onLeave(e.client); break;
                case net::EventType::Data:         onPacket(e.client, e.channel, e.data); break;
            }
        }
        simulate();
        network_.broadcast(serializeWorldState(), net::Channel::Unreliable);
    }

private:
    net::INetworkServer& network_;
};

int main() {
    auto network = net::createServer();
    if (!network->start(4242, 4243))
        return 1;
    GameServer server(*network);
    // fixed-timestep loop calling server.tick()
}
```

### Client

```cpp
auto network = net::createClient();
network->connect("127.0.0.1", 4242);

// each tick
network->send(serializeInput(sequence, keys), net::Channel::Unreliable);
for (auto& e : network->poll()) {
    if (e.type == net::EventType::Data)
        handlePacket(e.channel, e.data);
    else if (e.type == net::EventType::Disconnected)
        showConnectionLost();
}
```

### Testing without a network

Because game code depends on interfaces only, a `MockNetworkServer` returning scripted events can test server logic
without sockets.

---

## 8. Pitfalls

| Pitfall                                                                             | Consequence                                       | Fix                                                                      |
|-------------------------------------------------------------------------------------|---------------------------------------------------|--------------------------------------------------------------------------|
| Asio included in a public header                                                    | Slow builds, `<windows.h>` conflicts in game code | Asio only in `network/src/`                                              |
| Windows: UDP receive fails with `connection_reset` after sending to a closed client | Receive loop stops; server deaf to everyone       | Ignore `connection_reset` / `connection_refused` and restart the receive |
| Callback uses a destroyed `TcpSession`                                              | Crash                                             | `shared_from_this()` in every async handler                              |
| Socket used from the game thread                                                    | Data race                                         | `asio::post` to the network thread                                       |
| Receive buffer smaller than the datagram                                            | Datagram silently truncated                       | Buffer ≥ maximum packet size                                             |
| `system_clock` for timeouts                                                         | Timeouts break when the system clock changes      | `steady_clock`                                                           |
| Identifying clients by IP only                                                      | Players behind the same NAT are merged            | `ClientId` + token (or at least IP **and** port)                         |
