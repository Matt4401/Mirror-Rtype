# Getting Started

This guide sets up a development environment for Squacaron and the two games built on it.

## Prerequisites

- A C++23 compiler: `gcc` 13+ or `clang` 16+ on Linux, or MSVC on Windows.
- xmake, which is both the build system and the package manager. It downloads the dependencies into its own cache,
  so nothing is installed system-wide.

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
