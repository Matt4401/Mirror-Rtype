# Running the Game

Currently, the project compiles three main executables generated at the root of the repository, along with several test suites.

## Running the Tests

To run all automated tests (ECS, Plugins, Clock, Input, Scripting, Prefabs, Physics):

```bash
xmake test
```

Or run individual test suites:

```bash
xmake run test_plugins  # Plugins lifecycle, DAG, Mediator
xmake run test_ecs      # Core ECS Registry & Sparse Sets
xmake run test_clock    # Fixed-timestep simulation clock
xmake run test_input    # Action mapping and input normalization
xmake run test_physics  # Box2D physics simulation
xmake run test_scripting# Wren scripting integration
```

## Running the Server

The authoritative server runs headless (without a graphical window) and processes network inputs and ECS logic:

```bash
xmake run r-type_server
```

## Running the Client

The graphical client initializes the `engine::Engine` and loads modular plugins (`RenderPlugin`, `InputPlugin`, `PhysicsPlugin`, `RTypeGamePlugin`) via Raylib:

```bash
xmake run r-type_client
```

*(Note: At this prototype stage, network connections and IP configuration are hardcoded or rely on defaults. Future versions will accept CLI arguments).*
