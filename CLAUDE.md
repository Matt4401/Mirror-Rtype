# Mirror-Rtype

A highly decoupled, data-oriented C++23 game engine, built from scratch to power a networked multiplayer R-Type clone
(Epitech Advanced C++ project). Binaries: `r-type_server`, `r-type_client`.

The team follows **Track #1 – Advanced Architecture**: the engine must become a generic, standalone, reusable building
block. R-Type consumes it as a library, and a **second, different sample game** must be built on it to prove it.

## Stack
- C++23, xmake (build + package manager via `add_requires`: asio, gtest, wren, box2d, raylib)
- Rendering/input/audio: raylib — Networking: asio — Physics: box2d — Scripting: Wren
- Docs: MkDocs (`mkdocs.yml`, `docs/`) + Doxygen; protocol RFC in `docs/protocol/rfc.txt`
- Must run on Linux; Windows is a target too (cross-platform, cross-play) — no POSIX-only code without abstraction

## Commands
- Build: `xmake` (headless, no raylib: `xmake f --graphics=n && xmake`)
- Tests: `xmake f --tests=y && xmake && xmake test`
- Format: `clang-format -i <files>` (Google-based, 4 spaces, 120 cols)
- Lint: `clang-tidy -p . <file.cpp>` (`compile_commands.json` is auto-generated at the root)
- Docstring coverage: `python3 scripts/check_docstring_coverage.py --min-coverage 80 --dirs engine/src`
- Enable hooks: `git config core.hooksPath .githooks` (format + tidy as errors on staged files)

## Layout
- `engine/src/` — the engine, split into link-time modules (one static lib each):
  `engine_core` (ecs/), `engine_time` (ecs/time/), `engine_events` (events/, header-only),
  `engine_scripting` (scripting/, systems/script/), `engine_physics` (systems/physics/),
  `engine_render` (render/, systems/render/), `engine_input` (input/). `engine_all` is a phony aggregate.
- `network/src/` — asio-based UDP client/server + wire serialization (`Wire.hpp`)
- `shared/src/` — header-only interfaces shared by client/server (e.g. `INetworkServer`)
- `common/include/` — protocol data structs (`WorldState.hpp`)
- `server/src/`, `client/src/` — the R-Type game executables
- `scripts/` — Wren scripts (`scripts/core/*.wren`) + tooling scripts
- `tests/` — GoogleTest binaries, one `target` per suite in `tests/xmake.lua`
- `docs/` — architecture, protocol, comparative study, accessibility, dev guides

## Architecture rules (Track #1)
- **The engine knows nothing about R-Type.** No game rules, entity kinds, assets, or protocol packets in `engine/`.
  Game-specific code lives in `server/`, `client/` or a game module. If a feature needs game knowledge, expose a
  generic hook (component, event, script, config) instead.
- Keep modules independent: a module only depends on what it declares in its `xmake.lua` (`add_deps`).
  Never make `engine_core` depend on render/physics/scripting/network. Graphics-dependent code must stay behind
  the `graphics` option so the server builds headless.
- ECS first: data in components (plain structs), logic in systems, cross-system communication through
  `EventBus` / `CommandBuffer` — no direct system-to-system calls.
- Third-party types (raylib, box2d, asio, wren) stay inside their module's `.cpp`/private headers;
  public engine headers expose engine types only.
- Time-based logic uses the engine `Clock`/delta time, never frame count or CPU speed.
- Target features to grow toward: modularity (link-time, possibly runtime plugins), audio, resource/asset manager,
  message passing, scripting, dev console + CVars, in-game metrics/profiling, scene/level editor.

## Networking rules
- All in-game traffic is UDP with a **binary** protocol; TCP only for justified cases (`network/src/session/`).
- The server is authoritative and multithreaded; it must never block on clients and must survive client crashes.
- Every received packet is untrusted: validate sizes/bounds before reading; malformed input must never crash,
  over-allocate, or overflow.
- Any protocol change must update `docs/protocol/` (packets.md + rfc.txt).

## Conventions
- Templates: declaration in `.hpp`, implementation in `.tpp` included at the end of the header.
- `#pragma once`, namespaces per module (`ecs`, ...), `detail` namespace for internals.
- Doxygen `/** @brief ... */` on every public declaration in `engine/src` (≥80% coverage enforced).
- New source file → make sure it is matched by the target's `add_files` glob; new test suite → add a target in
  `tests/xmake.lua` with `add_tests`.
- Commits: Conventional Commits (`feat(ecs): ...`, `fix(network): ...`), see `CONTRIBUTING.md`.
  Work on `feat/<Name>` branches, merge through PRs into `main`.
- All documentation, comments, and commit messages are written in English.

## Never do
- Do not edit by hand: `build/`, `.xmake/`, `compile_commands.json`, the root `r-type_server` / `r-type_client` binaries
- Do not vendor third-party sources in the repo — add them with `add_requires` in `xmake.lua`
- Do not add R-Type-specific code to `engine/`
- Do not use an existing game engine or broad-scope framework (Unity, Godot, UE, ...)
- Do not commit directly to `main`
