---
name: lang-cpp
description: Modern C++ (C++17/20) best practices: do's, don'ts, design patterns and file architecture. Use it when writing, modifying or reviewing C++ code.
---

# Modern C++

Build: `xmake` · Tests: `xmake test`. First follow the standard and style already used in the project.
Details: [architecture.md](architecture.md) (file organisation) and [patterns.md](patterns.md) (design patterns).

## Do
- **RAII everywhere**: every resource (memory, file, mutex, socket) belongs to an object that releases it in its destructor.
- **Explicit ownership**: `std::unique_ptr` by default, `std::shared_ptr` only when ownership is really shared, raw pointer / reference to observe without owning.
- **Const-correctness**: `const` on methods that do not modify the object, on parameters and on locals that are not modified.
- **Parameter passing**: cheap types by value, large objects by `const&`, text reads through `std::string_view`, sequence reads through `std::span`, ownership transfer by value + `std::move`.
- **Strong types**: `enum class`, `std::optional` (absent value), `std::variant` (alternatives), `std::expected` (C++23) or a result type for expected errors.
- **Algorithms over hand-written loops**: `<algorithm>`, `<ranges>`; `auto` when the type is obvious or verbose.
- **Initialisation**: braces `{}`, members initialised in their declaration or the initialiser list, explicit `= default` / `= delete`.
- **Markers**: `override` (and `final`), `noexcept` on move and swap, `[[nodiscard]]` on results that must not be ignored, `constexpr` when the computation can happen at compile time.
- **Rule of zero**: do not hand-write destructor/copy/move if the members already manage their resources; otherwise follow the full rule of five.
- **Warnings**: build with `-Wall -Wextra -Wpedantic` (and sanitizers `-fsanitize=address,undefined` in debug); fix rather than silence.

## Don't
- Bare `new` / `delete` / `malloc`, C arrays, `char*` for text, `void*`.
- `using namespace` in a header (and `using namespace std` anywhere).
- Macros for constants or functions: prefer `constexpr`, `inline`, templates.
- C-style casts `(T)x`: use `static_cast`, and flag any `reinterpret_cast` / `const_cast`.
- Mutable globals, hidden singletons, shared state without synchronisation.
- Inheritance to reuse code: prefer composition; no multiple inheritance of implementations.
- Exceptions in hot paths not designed for them, or exceptions escaping a destructor.
- Unneeded includes in headers: prefer forward declarations (`class Foo;`).
- Implicit conversions (non-`explicit` single-argument constructors), signed/unsigned comparisons.
- Premature optimisation: measure first; do not add a dependency without asking.

## Naming and style
- Follow the project's formatter (`clang-format`). Types in `PascalCase`, functions/variables in `snake_case` or `camelCase` according to existing code, private members with a consistent prefix/suffix (`m_x` or `x_`), constants as `kPascalCase` or `UPPER_CASE` according to existing code.
- One header = one responsibility; use `#pragma once` or include guards consistently.
