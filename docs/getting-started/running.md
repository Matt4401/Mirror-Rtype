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
xmake run test_replication   # Network replication, with a codec independent of R-Type
xmake run test_server_e2e    # Real server and clients over loopback sockets
```

## Running the Server

The authoritative server runs headless (without a graphical window) and processes network inputs and ECS logic:

```bash
xmake run r-type_server               # listens on port 4242
xmake run r-type_server 5000          # listens on another port
xmake run r-type_server --logs        # also prints the debug logs
xmake run r-type_server --logs-colorful   # debug logs, colored and bold by severity and origin
xmake run r-type_server --level /tmp/test.toml   # plays another level file
```

Usage: `r-type_server [port] [--logs | --logs-colorful] [--level <path>]`, arguments in any order.

Every room plays the campaign: the `server/levels/level_*.toml` files, in the order of their names (see
[Level Files](../gameplay/levels.md)). The server looks up these files and the boss scripts (`scripts/`) from the
directory of its executable, the project root, so it can be started from any directory. A level file that cannot be
loaded is skipped with a warning; without any, the rooms play without a level. A level given with `--level` is played
alone and must load: otherwise the server stops with code 84. Its warnings are printed at start-up, which
makes `--level` handy to try a level file without touching the shipped one.

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

In code, log through `engine::log` (`engine/src/log/Logger.hpp`, xmake target `engine_log`), never `std::cout`:

```cpp
#include "log/Logger.hpp"

engine::log::debug("server", "client ", client, " joined room ", roomId);
```

## Running the Client

The graphical client connects to a server, so start one first:

```bash
xmake run r-type_client                   # connects to 127.0.0.1:4242
xmake run r-type_client 192.168.1.10 5000
```

Usage: `r-type_client [host] [port]`. It exits with code 84 if the server cannot be reached, and stops when the connection
is lost. Use `xmake run` rather than the binary at the root: raylib's libraries are found through the environment it sets.

## Playing a Game

1. Start the server, then a client. It finds no room and creates one.
2. Once the first client says `in room 1` in its terminal, start the others: each one joins the first room waiting for
   players, up to 4. Clients started at the very same moment may each create their own room.
3. Press **R** in each window to get ready: the game starts when everyone is ready. There is no lobby screen yet: the state
   of the room is printed in the terminal.
4. Move with WASD (ZQSD on AZERTY keyboards), the arrow keys or a gamepad.
