# Third-Party Libraries

This document explains the relevance of the technologies and external libraries we selected for the project, as required by the technical study.

---

## 1. Build System & Package Manager: xmake

| Criterion | CMake + Conan / vcpkg | **xmake (Our Choice)** |
| :--- | :--- | :--- |
| **Syntax** | Arcane, text-based | Clean, Lua-based |
| **Dependency Fetching** | Requires external setup | Automatic, built-in |
| **Speed** | Slow generation | Extremely fast |

**Justification:** While CMake is the industry standard, its syntax is notoriously complex. Combining it with Conan or vcpkg requires extensive manual setup for teammates. `xmake` provides an all-in-one solution that compiles C++ natively and downloads packages (Raylib, Asio, GTest) automatically with a single `xmake build` command, guaranteeing a friction-free experience for developers across Linux and Windows.

---

## 2. Graphics & Audio: Raylib

| Criterion | SFML | SDL2 | **Raylib (Our Choice)** |
| :--- | :--- | :--- | :--- |
| **Paradigm** | OOP (C++) | Pure C | Pure C / Procedural |
| **3D Support** | No (requires raw OpenGL) | Very limited | ✓ Excellent (Models, Shaders) |
| **Ease of Use** | Good | Verbose | ✓ Outstanding |

**Justification:** The subject explicitly allows Raylib as an alternative to SFML. Raylib is a modern, procedural C library that perfectly fits our Data-Oriented ECS design (unlike SFML which enforces heavy OOP structures). Furthermore, Raylib offers out-of-the-box support for 3D rendering, allowing us the flexibility to build a visually stunning "2.5D" R-Type (2D gameplay, 3D graphics) without writing raw OpenGL code.

---

## 3. Developer Tooling: Dear ImGui

**Justification:** The Track 1 Advanced Architecture requires in-game metrics, profiling, and developer consoles. Writing a GUI from scratch using Raylib primitives would take weeks of unnecessary effort. **Dear ImGui** is the absolute industry standard for C++ developer tools. By using the `rlImGui` bridge, we can instantly render complex inspector panels, performance graphs, and level editors seamlessly over our Raylib game window.

---

## 4. Physics: Box2D

**Justification:** R-Type is fundamentally a 2D game (Horizontal Shmup). Even if we use 3D graphics (2.5D) for visual flair, the actual gameplay logic strictly occurs on a 2D plane. **Box2D** is the industry standard for 2D rigid-body physics and collision detection. It is highly optimized, mathematically predictable, and integrates flawlessly with an ECS (where Box2D bodies are simply synchronized with our `Transform` components).
