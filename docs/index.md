# R-Type Engine

Welcome to the official documentation for the **R-Type** project.

This project focuses on the development of a multiplayer networked video game (client/server architecture) based on the classic arcade game R-Type, built entirely from scratch in modern C++.

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
