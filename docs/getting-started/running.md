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
xmake run r-type_server
```

## Running the Client

The graphical client connects to the server and handles rendering and input via Raylib:

```bash
xmake run r-type_client
```

*(Note: At this prototype stage, network connections and IP configuration are hardcoded or rely on defaults. Future versions will accept CLI arguments).*
