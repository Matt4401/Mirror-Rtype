---
name: build-xmake
description: Use the project's xmake build system. Use it to build, add a dependency or add a source file.
---

# Building with xmake

- Build: `xmake`; run: `xmake run <target>`; tests: `xmake test`.
- Configuration lives in `xmake.lua`: targets with `target("name")`, `add_files`, `add_includedirs`, `add_deps`.
- Dependencies with `add_requires("package")` then `add_packages("package")`; do not add a dependency without asking.
- Switch mode: `xmake f -m debug` or `xmake f -m release`.
- Do not edit the `.xmake/` or `build/` folders by hand; `xmake project -k compile_commands` generates `compile_commands.json`.

Project commands: build `xmake`, tests `xmake test`.
