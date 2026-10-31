# Running the Game

Currently, the project compiles three main executables generated at the root of the repository, along with several test suites.

## Running the Tests

To run all automated tests (ECS, Plugins, Clock, Input, Scripting, Prefabs, Physics):

```bash
xmake test
```

Or run individual test suites:

```bash
xmake run test_engine        # Engine and plugin lifecycle
xmake run test_plugin_graph  # Plugin dependency DAG
xmake run test_mediator      # Mediator service registry
xmake run test_ecs           # Core ECS Registry & Sparse Sets
xmake run test_clock         # Fixed-timestep simulation clock
xmake run test_input         # Action mapping and input normalization
xmake run test_physics       # Box2D physics simulation
xmake run test_scripting     # Wren scripting integration
```

## Running the Server

The authoritative server runs headless (without a graphical window) and processes network inputs and ECS logic:

```bash
xmake run r-type_server               # listens on port 4242
xmake run r-type_server 5000          # listens on another port
xmake run r-type_server --logs        # also prints the debug logs
xmake run r-type_server --logs-colorful   # debug logs, colored and bold by severity and origin
```

Usage: `r-type_server [port] [--logs | --logs-colorful]`, arguments in any order.

Each line starts with the time since startup in seconds, the level and the subsystem:

```text
[    0.000] INFO  [server] listening on port 4242
[    0.508] DEBUG [network] TCP connection from 127.0.0.1:45814 accepted as client 1
[    0.509] INFO  [server] client 1 connected
```

- `INFO` (stdout): startup, stop, connections and disconnections. Always printed.
- `WARN` / `ERROR` (stderr): refused connections, timeouts, invalid payloads, port already taken. Always printed.
- `DEBUG` (stdout): only with `--logs` or `--logs-colorful`. Received packets (size and channel), rooms created and destroyed, players
  joining and leaving a room, UDP binding and dropped datagrams.

With `--logs-colorful`, the timestamp is dim, the level is colored by severity (gray `DEBUG`, bold green `INFO`,
bold yellow `WARN`, bold white on red `ERROR`), the tag is bold in a color picked from its name (`server` and `network`
always get the same one), and the text of warnings and errors is yellow and red.

In code, log through `rtype::log` (`log/src/Logger.hpp`, xmake target `log`), never `std::cout`:

```cpp
#include "Logger.hpp"

rtype::log::debug("server", "client ", client, " joined room ", roomId);
```

## Running the Client

The graphical client initializes the `engine::Engine` and loads modular plugins (`RenderPlugin`, `InputPlugin`, `PhysicsPlugin`, `RTypeGamePlugin`) via Raylib:

```bash
xmake run r-type_client
```

*(Note: At this prototype stage, network connections and IP configuration are hardcoded or rely on defaults. Future versions will accept CLI arguments).*
