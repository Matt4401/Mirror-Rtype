# Squacaron

Squacaron is a C++23 game engine written from scratch for the EPITECH Advanced C++ project. Two games run on it:

- **R-Type**, a networked multiplayer shoot'em up with an authoritative server ([gameplay](gameplay/levels.md)).
- **RLSS**, a 2D car football game, built to check that the engine does not depend on R-Type ([RLSS](rlss/index.md)).

## Core Architecture

The project is structured around several key components:

- **Server (Authoritative)**: Responsible for the core game logic, physics, collision detection, enemy AI, and strict validation of all player actions.
- **Client**: Handles graphical rendering, audio playback, user input management, and client-side prediction for a smooth gameplay experience.
- **Game Engine & Plugins**: A custom Entity-Component-System engine powered by a DAG-based plugin dependency system (`engine::IPlugin`, `engine::PluginGraph`) and a service Mediator (`engine::Mediator`), designed to be generic, standalone, and reusable across multiple game titles (Track #1).

Use the navigation menu to explore detailed documentation regarding the system architecture, network protocols, comparative studies, and the auto-generated API reference.

## Contributors

- Alexis Clemot
- Matthieu Coraleau
- Tom Gatin
- Tristan Fragnaud
