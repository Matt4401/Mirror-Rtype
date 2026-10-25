# Running the Game

Currently, the project compiles three main executables generated at the root of the repository.

## Running the Tests

To ensure the core ECS engine is functioning correctly:

```bash
xmake run test_ecs
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

The graphical client connects to the server and handles rendering and input via Raylib:

```bash
xmake run r-type_client
```

*(Note: At this prototype stage, network connections and IP configuration are hardcoded or rely on defaults. Future versions will accept CLI arguments).*
