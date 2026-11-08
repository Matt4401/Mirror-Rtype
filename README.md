<!-- markdownlint-disable MD033 -->
# Squacaron

[![Cross-platform build](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build.yml/badge.svg)](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build.yml)
[![macOS build](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build-macos.yml/badge.svg)](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build-macos.yml)
[![Documentation](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/docs.yml/badge.svg)](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/docs.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![Build System](https://img.shields.io/badge/Build%20System-xmake-red)](https://xmake.io/)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20macOS-informational)

**Squacaron** is a decoupled, data-oriented **C++23 game engine** written from scratch. It runs two games: a networked multiplayer **R-Type** clone and **RLSS**, a 2D car football game. It was built for the EPITECH Advanced C++ project.

---

## Table of Contents

- [About the Project](#about-the-project)
- [Key Features](#key-features)
- [Tech Stack & Dependencies](#tech-stack--dependencies)
- [Quick Start](#quick-start)
  - [Prerequisites](#prerequisites)
  - [Installation & Build](#installation--build)
  - [Running the Game](#running-the-game)
- [Second Game: RLSS](#second-game-rlss)
- [Project Architecture](#project-architecture)
- [Network Protocol](#network-protocol)
- [Testing & Quality Assurance](#testing--quality-assurance)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Contributors](#contributors)

---

## About the Project

This is the third-year EPITECH Advanced C++ project. We wrote a generic game engine built on an Entity-Component-System (ECS), then a networked 2D shooter (R-Type) on top of it, with an authoritative server. A second game, RLSS, runs on the same engine to check that the engine does not depend on R-Type.

---

## Key Features

- ECS with sparse/dense component storage and listeners on component add and remove.
- Authoritative server: it validates the inputs, runs the collisions and sends the world state over UDP.
- Client prediction, reconciliation and interpolation, so the game stays playable with latency.
- 2D physics and collision detection with Box2D.
- Gameplay scripts in Wren.
- Builds on Linux, Windows and macOS with `xmake`.

---

## Tech Stack & Dependencies

The project is fully self-contained using **xmake**, which automatically manages external dependencies:

| Dependency | Purpose |
| :--- | :--- |
| [xmake](https://xmake.io/) | Fast cross-platform build utility and package manager |
| [Asio](https://think-async.com/Asio/) | Asynchronous network I/O for client-server UDP/TCP communication |
| [Raylib](https://www.raylib.com/) | Windowing, 2D hardware-accelerated rendering, and audio (Client) |
| [Box2D](https://box2d.org/) | 2D physics simulation and collision resolution |
| [Wren](https://wren.io/) | Small, fast, class-based concurrent scripting language |
| [GoogleTest](https://github.com/google/googletest) | Modern unit testing and test coverage framework |

---

## Quick Start

> Platform-specific setup and build options are in the [Getting Started documentation](docs/getting-started/index.md).

### Prerequisites

- A C++23 compiler (`gcc` 13+, `clang` 16+, or MSVC 2022+).
- [xmake](https://xmake.io/), a recent version.

<details>
<summary><b>Installing xmake</b></summary>

- **Linux / macOS**:

  ```bash
  curl -fsSL https://xmake.io/shget.text | bash
  ```

- **Windows (PowerShell)**:

  ```powershell
  Invoke-Expression (Invoke-Webrequest 'https://xmake.io/psget.text' -UseBasicParsing).Content
  ```

</details>

<details>
<summary><b>Installing MSVC on Windows</b></summary>

On Windows, xmake builds with MSVC. If MSVC is not installed, `xmake f` fails because it cannot find a
compiler. Install Visual Studio 2022 with the **Desktop development with C++** workload (MSVC compiler + Windows SDK).

- **Visual Studio Community (IDE + compiler)**, from PowerShell:

  ```powershell
  winget install --id Microsoft.VisualStudio.2022.Community --override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
  ```

- **Build Tools only (compiler without the IDE)**, from PowerShell:

  ```powershell
  winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
  ```

- **Visual Studio already installed, but without the C++ workload**: add it from PowerShell:

  ```powershell
  & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe" modify `
      --installPath "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community" `
      --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive
  ```

  Or open the **Visual Studio Installer**, click **Modify** and check **Desktop development with C++**.

Then open a new terminal and run `xmake f -c`. xmake detects MSVC by itself.

</details>

### Installation & Build

The [Building Guide](docs/getting-started/building.md) has the details.

```bash
# 1. Clone the repository
git clone https://github.com/Matt4401/Mirror-Rtype.git
cd Mirror-Rtype

# 2. Configure the build environment (automatically resolves dependencies)
xmake f -c

# 3. Build all targets (Engine, Server, Client, Tests)
xmake build
```

### Running the Game

For additional runtime details, consult the **[Running Guide](docs/getting-started/running.md)**.

```bash
# Start the authoritative game server
xmake run r-type_server

# In a separate terminal, launch the graphical client
xmake run r-type_client

# Run the test suite
xmake run test_ecs
```

---

## Second Game: RLSS

RLSS is a 2D, side-view car football game (cars, boost, a ball, two goals) whose car and ball physics follow
Rocket League. We built it on the same engine to show that Squacaron does not depend on R-Type: it lives in `rlss/`,
links only the engine modules it needs, and nothing in `engine/` was changed for it. The car physics are written in
Wren (`rlss/scripts/`) and hot-reload while the server runs.

```bash
xmake                          # also builds rlss_server and rlss_client
./rlss_server [port]           # default port 4250
./rlss_client [host] [port]    # default 127.0.0.1 4250, one per player
```

Run both from the repository root (scripts and assets are loaded from `rlss/`). Controls, rules and the module
layout are in the **[RLSS documentation](docs/rlss/index.md)**.

---

## Project Architecture

The codebase is organized into modular libraries and applications:

```text
.
├── client/        # Graphical client implementation (Raylib rendering, input, interpolation)
├── common/        # Game protocol: Protobuf schema and codec
├── docs/          # Complete MkDocs & Doxygen documentation
├── engine/        # Squacaron, the game-agnostic engine:
│   ├── ecs/       # Entity-Component-System (Registry, SparseSet, Systems)
│   ├── events/    # Decoupled Event Bus & Dispatcher
│   ├── net/       # Transport (Asio in net/asio/), NetworkPlugin & replication
│   ├── physics/   # Box2D physics integration
│   ├── scripting/ # Wren scripting integration
│   └── time/      # High-resolution game clock & tick management
├── game/          # R-Type code shared by client and server (replication codec, ship movement)
├── rlss/          # Second game on the engine: 2D car football (server, client, Wren scripts)
├── server/        # Authoritative server (lobby, one engine per room)
└── tests/         # Unit and integration test suites (GoogleTest)
```

Each module is described in the [Architecture Overview](docs/architecture/overview.md).

---

## Network Protocol

Lobby and session messages go over TCP; in-game inputs and world snapshots go over UDP. Both carry a small binary
header followed by a Protobuf payload. The server checks every input it receives before applying it, and sends the
world state to each client in its room.

Specifications:

- **[Network RFC](docs/protocol/rfc.txt)**
- **[Packet Definitions](docs/protocol/packets.md)**
- **[Transport & Connection](docs/protocol/transport.md)**

---

## Testing & Quality Assurance

The tests are GoogleTest suites, one binary per module:

```bash
# Run all automated tests
xmake test

# Run specific test suites
xmake run test_ecs
xmake run test_physics
xmake run test_scripting
xmake run test_clock
```

### Pre-commit Hooks

Ensure code formatting (`clang-format`) and static analysis (`clang-tidy`) are verified before committing:

```bash
# Configure repository git hooks
git config core.hooksPath .githooks

# Or install with python pre-commit
pre-commit install
```

---

## Documentation

The documentation is hosted at **<https://matt4401.github.io/Mirror-Rtype/>** (rebuilt on every push to `main`).
It is written with MkDocs Material and Doxygen. To build it locally:

```bash
# Install documentation dependencies
pip install mkdocs-material mkdoxy pymdown-extensions mkdocs-section-index

# Serve documentation locally
mkdocs serve
```

Browse the documentation topics:

- **[Getting Started Guide](docs/getting-started/index.md)**
- **[Scripting Guide](docs/getting-started/scripting-guide.md)**
- **[Architecture Guide](docs/architecture/overview.md)**
- **[Comparative Studies](docs/comparative-study/libraries.md)**

---

## Contributing

Commits follow [Conventional Commits](https://www.conventionalcommits.org/) and go through pull requests; see
[CONTRIBUTING.md](CONTRIBUTING.md), the [development workflow](docs/development/workflow.md) and the
[coding style](docs/development/coding-style.md).

---

## Contributors

<table>
  <tr>
    <td align="center" width="25%">
      <a href="https://github.com/Alexis-Clemot">
        <img src="https://github.com/Alexis-Clemot.png" width="100px;" alt="Alexis Clemot" style="border-radius: 50%;"/><br />
        <sub><b>Alexis Clemot</b></sub>
      </a><br />
      <sub><a href="https://github.com/Alexis-Clemot">@Alexis-Clemot</a></sub>
    </td>
    <td align="center" width="25%">
      <a href="https://github.com/Matt4401">
        <img src="https://github.com/Matt4401.png" width="100px;" alt="Matthieu Coraleau" style="border-radius: 50%;"/><br />
        <sub><b>Matthieu Coraleau</b></sub>
      </a><br />
      <sub><a href="https://github.com/Matt4401">@Matt4401</a></sub>
    </td>
    <td align="center" width="25%">
      <a href="https://github.com/TomGatin">
        <img src="https://github.com/TomGatin.png" width="100px;" alt="Tom Gatin" style="border-radius: 50%;"/><br />
        <sub><b>Tom Gatin</b></sub>
      </a><br />
      <sub><a href="https://github.com/TomGatin">@TomGatin</a></sub>
    </td>
    <td align="center" width="25%">
      <a href="https://github.com/TristanFragnaud">
        <img src="https://github.com/TristanFragnaud.png" width="100px;" alt="Tristan Fragnaud" style="border-radius: 50%;"/><br />
        <sub><b>Tristan Fragnaud</b></sub>
      </a><br />
      <sub><a href="https://github.com/TristanFragnaud">@TristanFragnaud</a></sub>
    </td>
  </tr>
</table>
