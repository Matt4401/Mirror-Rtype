# Architecture Overview

## 1. High-Level Architecture

The project is structured into three primary tiers:

1. **Engine Core:** An agnostic standalone library providing ECS, event dispatching, rendering wrappers (Raylib), and networking primitives (Asio).
2. **Authoritative Server:** Hosts game simulation sessions, runs logical systems, enforces validation, and synchronizes states.
3. **Graphical Client:** Renders the world, handles local inputs, performs interpolation and client-side prediction, and queries user devices.

```mermaid
graph TB
    subgraph Engine["Engine (Shared Core)"]
        ECS["ECS Core (Registry, Sparse Sets)"]
        NetWrapper["Networking Primitives (Asio)"]
        Events["Event Bus"]
    end

    subgraph ServerApp["r-type_server"]
        ServerECS["Server Registry"]
        ServerLogic["Game Logic & Rules Systems"]
        ServerNet["Server Network Manager (UDP / TCP)"]
    end

    subgraph ClientApp["r-type_client"]
        ClientECS["Client Registry"]
        Renderer["Render System (Raylib)"]
        Input["Input System"]
        ClientNet["Client Network Manager"]
    end

    Engine --> ServerApp
    Engine --> ClientApp

    ServerNet <-->|"Binary Protocol (UDP / TCP)"| ClientNet
```

## 2. Client-Server Synchronization Model

The game follows an **Authoritative Server** architecture:

- **Client:** Captures player input and sends it across UDP datagrams. It renders local feedback and reconciles discrepancies when server updates arrive.
- **Server:** Runs at a fixed simulation tick rate, advances the game state, handles collision and damage calculations, and broadcasts compressed world updates.

```mermaid
sequenceDiagram
    participant Client
    participant Server

    Client->>Server: UDP: Player Input (Move, Shoot)
    Note over Server: Input processed by MovementSystem
    Note over Server: CollisionSystem resolves interactions
    Server->>Client: UDP: World Snapshot / Entity Updates
    Note over Client: Local ECS state synchronized
    Note over Client: RenderSystem draws updated frame
```
