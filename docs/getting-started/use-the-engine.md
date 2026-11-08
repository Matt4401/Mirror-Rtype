# Use the Engine in Your Own Game

Squacaron builds on its own, without R-Type. A game depends on it like on any other library: through an xmake package.
This page shows the smallest game that does so.

## Build the engine alone

The `engine/` directory is a complete xmake project. From the repository root:

```bash
cd engine
xmake f -P . --tests=y -y
xmake build -P .
xmake test -P .
```

`-P .` tells xmake that `engine/` is the project root. Without it, xmake finds the `xmake.lua` of the repository and
builds the games as well.

`xmake install -P . -o <dir>` copies the headers to `<dir>/include/squacaron/` and the static libraries to
`<dir>/lib/`.

To build without the render and audio modules (and without raylib), add `--graphics=n`.

## The `squacaron` package

`xmake-repo/` is a local xmake repository with a `squacaron` package built from `engine/`. A game declares it in its
`xmake.lua`:

```lua
set_languages("c++23")
add_rules("mode.debug", "mode.release")

add_repositories("local-repo path/to/Mirror-Rtype/xmake-repo")
add_requires("squacaron", {configs = {graphics = true}})

target("my_game")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("squacaron")
```

The package brings the engine libraries and their dependencies (asio, wren, box2d, toml++ and raylib when
`graphics` is on). Headers are included as the engine modules name them: `core/Engine.hpp`, `ecs/Registry.hpp`,
`events/EventBus.hpp`...

## A minimal game

```cpp
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"

class HelloPlugin : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Hello"; }

    void update(engine::Engine& engine, const ecs::time::Time& time) override {
        (void)engine;
        (void)time;
        // Game logic: read and write components through the registry.
    }
};

int main() {
    engine::Engine engine;
    engine.addPlugin<HelloPlugin>();
    engine.run();
}
```

A headless program like this one has no window to close it, so it runs until `engine.stop()` is called. Add
`render::RenderPlugin` to get a window, or build a server out of `engine_network` and `engine_network_asio` (see
[Add the Network to a Game](networked-game.md)).

## Package mode inside this repository

The R-Type and RLSS targets build against the engine sources by default, so an engine change is always picked up. To
build them the way an outside game would, against the installed package:

```bash
xmake f --engine_package=y -y
xmake require --force squacaron
xmake build
```

xmake does not rebuild a local package when its sources change. Run `xmake require --force squacaron` after every
engine change in this mode. The `engine-package` job of the CI does it on each pull request.
