# C++ file architecture

Keep the existing layout if there is one. Otherwise, recommended structure:

```
project/
├── CMakeLists.txt | xmake.lua      # root build
├── include/<project>/              # public headers (exposed API)
│   └── module.hpp
├── src/                            # implementation and private headers
│   ├── module.cpp
│   └── internal/…
├── tests/                          # one test file per module
│   └── module_test.cpp
├── apps/ or examples/              # executables
├── third_party/                    # vendored dependencies (do not edit)
└── docs/
```

## Rules
- **Public vs private headers**: only `include/` is visible to library users; include with the prefixed path `#include <project/module.hpp>`.
- **One class (or a small cohesive group) per `.hpp` / `.cpp` pair**, named after the main class.
- **Header**: declarations, templates, small `inline` functions. **Source**: definitions, heavy includes.
- **Include order**: the matching header first, then the project, then third-party libraries, then the standard library — each group sorted.
- **One-directional dependencies** between modules (no cycles); split into libraries/targets when a module grows.
- **Namespaces** mirroring the folders; `namespace detail { … }` for what must not be used from outside.
- **Templates**: implementation in the header (or a `.tpp` included at the end of the header).
- **A source file added = declared in the build system**.
- **Keep logic out of `main`**: a minimal `main.cpp` that calls a testable library.
