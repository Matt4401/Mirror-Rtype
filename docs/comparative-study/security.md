# Security & Integrity

This page addresses the security, safety, and integrity aspects of both the networked game state and the engine's memory management.

---

## 1. Network Security: The Authoritative Server

Multiplayer games are highly susceptible to cheating if the client is trusted with game logic.

### Vulnerability: Client Authority

If a client computes its own position and sends it to the server (`"I am at X: 500, Y: 200"`), malicious users can use memory editors (like Cheat Engine) or modified clients to teleport anywhere or instantly kill enemies.

### Our Solution: Strict Server Authority

The server is the absolute source of truth.

- **Dumb Terminals:** Clients only transmit raw user inputs (e.g., `"Key UP pressed"` or `"Action SHOOT triggered"`).
- **Validation:** The server applies these inputs to its internal ECS simulation, validates movement speeds, applies damage, and broadcasts the definitive new world state back to all clients.
- **Integrity:** It is physically impossible for a client to teleport, because the server controls the `Transform` and `Velocity` components.

---

## 2. Protocol Integrity: UDP Hardening

UDP is a connectionless protocol. It does not guarantee delivery, order, or data integrity out-of-the-box.

### Vulnerability: Malformed Packets & Buffer Overflows

Sending raw C-strings or unbounded arrays over UDP is a massive vulnerability. An attacker could send a 10,000-byte packet designed to overflow a read buffer and execute arbitrary code on the server.

### Our Solution: Binary Protocol with Fixed-Size Structures

Our binary protocol (as described in the Network RFC) enforces strict, fixed-size structs.

- Variable-length data is preceded by validated length headers.
- Sequence numbers ensure that old or reordered packets are discarded, preventing "replay attacks" where an attacker captures and resends an old movement packet to alter the game state.

---

## 3. Engine Safety: Modern C++ Memory Management

A significant portion of security vulnerabilities in native applications stems from memory mismanagement (Use-After-Free, Double-Free, memory leaks).

### Vulnerability: Raw Memory Allocation

Using `new` and `delete` manually in an engine handling thousands of dynamic entities is inherently unsafe and leak-prone.

### Our Solution: RAII and Smart Containers

The ECS architecture heavily leverages C++23 features to guarantee memory safety:

- **`std::unique_ptr`**: Used inside the `Registry` to manage the lifetime of type-erased `ISparseArray` objects. When the registry goes out of scope, all component arrays are automatically and safely destroyed.
- **Contiguous Vectors**: Components are stored in `std::vector`. Bounds checking and iterator safety are handled by the STL, severely reducing the risk of accidental buffer overruns during system iteration.
