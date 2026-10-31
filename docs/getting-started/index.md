# Getting Started

Welcome to the R-Type engine project! This guide will help you set up your development environment.

## Prerequisites

Our project utilizes modern tools to guarantee cross-platform compatibility without polluting your OS system paths.

- **C++23 Compiler**: `gcc` 13+ or `clang` 16+ on Linux, or MSVC on Windows.
- **xmake**: We use xmake as both our build system and package manager. It automatically fetches all dependencies in an isolated environment.

### Installing xmake

**Linux / macOS:**

```bash
curl -fsSL https://xmake.io/shget.text | bash
```

**Windows (PowerShell):**

```powershell
Invoke-Expression (Invoke-Webrequest 'https://xmake.io/psget.text' -UseBasicParsing).Content
```

## Next Steps

- [Building the Project](building.md): How to configure and build targets with xmake.
- [Running the Game & Tests](running.md): How to run the client, server, and test suites.
- [Plugins Architecture](../architecture/plugins.md): How to create and register modular engine plugins.
- [Scripting Guide](scripting-guide.md): How to write gameplay scripts in Wren.
