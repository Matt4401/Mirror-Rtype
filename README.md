# R-Type Game Engine

A highly decoupled, data-oriented C++23 game engine, built from scratch to power a networked multiplayer R-Type clone.

## Purpose

This project is part of the Advanced C++ curriculum in the 3rd year of EPITECH. The goal is to build a robust, generic game engine utilizing an Entity-Component-System (ECS) architecture, and to implement a networked 2D shooter (R-Type) on top of it using an Authoritative Server model.

## Dependencies

The project is fully self-contained using a modern C++ package manager. It relies strictly on:

- **xmake**: Build system and package manager.
- **Raylib**: Window creation, rendering, and audio (Client-side only).
- **Asio**: Asynchronous networking for UDP/TCP communication.
- **GoogleTest (gtest)**: Unit testing framework.

## Supported Platforms

- Linux
- Windows (Visual Studio / MSVC)

## Building the Project

Ensure you have [xmake](https://xmake.io/) installed on your system.

```bash
# Configure the project
xmake f -c

# Build all targets (Engine, Server, Client)
xmake build
```

## Running

The binaries are generated in the root of the project directory.

```bash
# Run the authoritative server
xmake run r-type_server

# Run the graphical client
xmake run r-type_client

# Run the engine test suite
xmake run test_ecs
```

## Architecture Overview

- The `engine/` library contains the purely agnostic ECS, Event Bus, and abstractions.
- The `server/` binary depends on `engine` and `Asio` to handle game logic and network broadcasting.
- The `client/` binary depends on `engine`, `Asio`, and `Raylib` to handle input, interpolation, and rendering.

## Contributors

- Alexis Clemot
- Matthieu Coraleau
- Tom Gatin
- Tristan Fragnaud
