<!-- markdownlint-disable MD033 -->
# R-Type Game Engine

[![Cross-platform build](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build.yml/badge.svg)](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build.yml)
[![macOS build](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build-macos.yml/badge.svg)](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/build-macos.yml)
[![Documentation](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/docs.yml/badge.svg)](https://github.com/Matt4401/Mirror-Rtype/actions/workflows/docs.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![Build System](https://img.shields.io/badge/Build%20System-xmake-red)](https://xmake.io/)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20macOS-informational)

A highly decoupled, data-oriented **C++23 game engine** built from scratch to power a networked multiplayer **R-Type** clone, developed as part of the EPITECH curriculum.

---

## Table of Contents

- [About the Project](#about-the-project)
- [Key Features](#key-features)
- [Tech Stack & Dependencies](#tech-stack--dependencies)
- [Quick Start](#quick-start)
  - [Prerequisites](#prerequisites)
  - [Installation & Build](#installation--build)
  - [Running the Game](#running-the-game)
- [Project Architecture](#project-architecture)
- [Network Protocol](#network-protocol)
- [Testing & Quality Assurance](#testing--quality-assurance)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Contributors](#contributors)

---

## About the Project

This project is part of the **Advanced C++** curriculum in the 3rd year of EPITECH. The objective is to design a generic, modular, and performant game engine utilizing an **Entity-Component-System (ECS)** architecture, and to implement a networked 2D shooter (**R-Type**) on top of it using an **authoritative server model**.

---

## Key Features

- **Custom ECS Architecture**: Cache-friendly sparse/dense storage with type-safe entity management and reactive lifecycle listeners.
- **Authoritative Server**: Server-authoritative state synchronization, input validation, and collision management over UDP.
- **Client Interpolation & Prediction**: Smooth rendering and lag compensation for high-latency connections.
- **2D Physics Engine**: Rigid-body physics and collision detection powered by Box2D.
- **Scripting Engine**: Extensible game logic and event scripting using Wren.
- **Cross-Platform**: Seamless builds across Linux, Windows, and macOS via `xmake`.

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

> **Full Guide**: For detailed platform-specific setup and options, see the **[Getting Started Documentation](docs/getting-started/index.md)**.

### Prerequisites

- A **C++23** compliant compiler (`gcc` 13+, `clang` 16+, or MSVC 2022+).
- **[xmake](https://xmake.io/)** (latest version recommended).

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

### Installation & Build

For comprehensive instructions, check the **[Building Guide](docs/getting-started/building.md)**.

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

## Project Architecture

The codebase is organized into modular libraries and applications:

```text
.
├── client/        # Graphical client implementation (Raylib rendering, input, interpolation)
├── common/        # Common headers and shared utility definitions
├── docs/          # Complete MkDocs & Doxygen documentation
├── engine/        # Agnostic core game engine:
│   ├── ecs/       # Entity-Component-System (Registry, SparseSet, Systems)
│   ├── events/    # Decoupled Event Bus & Dispatcher
│   ├── physics/   # Box2D physics integration
│   ├── scripting/ # Wren scripting integration
│   └── time/      # High-resolution game clock & tick management
├── network/       # Network transport abstraction & packet serialization
├── server/        # Authoritative server (game simulation, network broadcasting)
├── shared/        # Shared game components, systems, and game protocol definitions
└── tests/         # Unit and integration test suites (GoogleTest)
```

For an in-depth breakdown of each subsystem, see the **[Architecture Overview](docs/architecture/overview.md)**.

---

## Network Protocol

The game uses a lightweight binary protocol over UDP designed for low-latency transmission:

- Dedicated packet IDs with structured payloads.
- Client inputs are validated server-side to prevent desynchronization.
- State updates are broadcast to connected clients.

Read the specifications:

- **[Network RFC](docs/protocol/rfc.txt)**
- **[Packet Definitions](docs/protocol/packets.md)**
- **[Transport & Connection](docs/protocol/transport.md)**

---

## Testing & Quality Assurance

We maintain strict test coverage and code quality standards:

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

Full documentation is available via **MkDocs Material** and **Doxygen**:

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

Contributions are welcome! Please follow our commit standards and guidelines before submitting a pull request:

- Review **[CONTRIBUTING.md](CONTRIBUTING.md)** for our [Conventional Commits](https://www.conventionalcommits.org/) standards and pull request workflows.
- Check our **[Development Workflow](docs/development/workflow.md)** and **[Coding Style Guide](docs/development/coding-style.md)**.

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
