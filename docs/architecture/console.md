# Console and CVars

Console variables (CVars) are engine and game settings that can be read and changed by name while the game runs:
`sv_gravity 12` changes the gravity on the next physics step, without a restart. The console interpreter runs
commands (`help`, `set`, `spawn alien`...) typed as text.

| Module         | Contents                                                                                | Depends on   |
|----------------|-----------------------------------------------------------------------------------------|--------------|
| `engine_tools` | `CVarManager`, `Console`, `splitCommandLine`, built-in commands, `ConsolePlugin`        | core, events |

Sources are split by role: `tools/cvars/` holds the `CVarManager`, `tools/console/` the interpreter, the built-in
commands and the plugin. The module draws nothing and does not use raylib: a server uses it as well as a client. The in-game terminal window
(toggled with `~` / `²`) is a separate UI plugin built on top of this one.

## Registering and reading a CVar

```cpp
#include "tools/cvars/CVarManager.hpp"
#include "tools/console/ConsolePlugin.hpp"

engine.addPlugin<tools::ConsolePlugin>(tools::ConsoleConfig{.allowCheats = true});

// In the init of the plugin that owns the setting:
auto& cvars = engine.service<tools::CVarManager>();
_gravity = cvars.add("sv_gravity", 9.8F, "World gravity (m/s^2)",
                     tools::CVarOptions{.flags = tools::CVarFlag::Cheat,
                                        .range = tools::CVarRange{.min = -100.0, .max = 100.0}});

// Every step: a pointer read, so console changes apply immediately.
world.setGravity(_gravity.get());
```

- **Types.** `bool`, `int`, `float` or `std::string`, fixed by the default value.
- **Names.** 1 to 64 characters among `a-z`, `0-9`, `_` and `.`, starting with a letter or `_`. Use a prefix per area:
  `sv_` (server simulation), `r_` (rendering), `net_` (network), `cl_` (client).
- **Registering twice** with the same type returns the existing variable and keeps its value; with another type, an
  invalid name or an out-of-range default, `add` throws `std::invalid_argument` (programming errors).
- **Lifetime.** Variables are never removed: a `CVarRef` stays valid as long as the `CVarManager`.
- A plugin registering cvars in its `init` declares `builder.optionally<tools::ConsolePlugin>()` so the console is
  initialized first, and checks `engine.mediator().hasService<tools::CVarManager>()`: the engine still works
  without a console.

## Flags

| Flag       | Meaning                                                                                                      |
|------------|--------------------------------------------------------------------------------------------------------------|
| `Cheat`    | Changed from the console only while cheats are allowed. Disabling cheats resets every cheat cvar to its default. |
| `ReadOnly` | Shown by the console, changed only by code (`CVarRef::set`).                                                  |
| `Archived` | Meant to be saved between sessions; the plugin owning the persistence reads this flag.                         |

Cheats are **disallowed by default** (`ConsoleConfig::allowCheats = false`), which is what a competitive server
keeps. Cheat commands (`Command::cheat`) follow the same rule.

## Untrusted input

Everything typed in the console is untrusted:

- **Lines** longer than 1024 bytes, with more than 64 words, with control characters (other than tabs) or with an
  open quote are refused before anything runs.
- **Values** are parsed strictly: `12abc`, `0x10`, `nan`, `inf` or an integer overflow are refused, the old value is
  kept. `bool` accepts `true/false`, `1/0`, `on/off`, `yes/no`.
- **Strings** are limited to 256 characters without control characters, so a value can never inject terminal escape
  sequences into the output.
- **Bounds** (`CVarRange`) are checked for console changes and code changes alike.
- **Errors** are clear messages in the output: `Unknown cvar 'sv_gravty'`, `'sv_gravity': expected a number`,
  `'sv_gravity' is cheat-protected and cheats are disabled`, `Usage: set <cvar> <value>`.
- A command handler that throws is reported as `'<name>' failed: <what>`; the console keeps running.

## Console syntax

| Line                            | Effect                                                  |
|---------------------------------|---------------------------------------------------------|
| `sv_gravity`                    | Prints the value, default, range, flags and description |
| `sv_gravity 12`                 | Changes the cvar (same as `set sv_gravity 12`)          |
| `set name "Big Boss"`           | Double quotes group words; `\"` and `\\` escape inside  |
| `r_wireframe 1; r_draw_colliders on` | `;` separates statements                           |
| `echo hi // comment`            | `//` starts a comment                                   |

Command names are case-insensitive. A command hides a cvar of the same name.

## Built-in commands

| Command              | Effect                                                     |
|----------------------|------------------------------------------------------------|
| `help [name]`        | Lists the commands, or describes one command or cvar       |
| `clear`              | Empties the output log                                     |
| `echo <text...>`     | Prints its arguments                                       |
| `cvar_list [prefix]` | Lists the cvars whose name starts with the prefix          |
| `set <cvar> <value>` | Changes a cvar                                             |
| `reset <cvar>`       | Restores the default value                                 |
| `toggle <cvar>`      | Flips a `bool` cvar                                        |

Game commands are added by the game, never by the engine:

```cpp
auto& console = engine.service<tools::Console>();
console.addCommand("spawn", {.usage = "<prefab>",
                             .description = "Spawns a prefab in front of the player",
                             .handler = [this](tools::CommandContext& ctx) { spawn(ctx.args().front()); },
                             .minArgs = 1,
                             .maxArgs = 1,
                             .cheat = true,
                             .completer = [this](const tools::Console&, std::size_t, std::string_view) {
                                 return prefabNames();
                             }});
```

`CommandContext::print` writes a line, `CommandContext::error` writes an error and marks the command as failed.

## Completion, history and output

- `Console::complete(line)` completes the word being typed: command and cvar names first, then the arguments a
  command's `completer` suggests (cvar names for `set`, `reset`, `toggle`, `cvar_list`; `true`/`false` for a `bool`
  cvar). A single candidate is completed with a trailing space; several candidates complete their shared prefix and
  are returned for the UI to list.
- `Console::history()` keeps the last 100 executed lines, without consecutive duplicates.
- `Console::output()` keeps the last 512 output lines (echoed input, results, errors) for a UI or a terminal to show.

## ConsolePlugin

- Owns the `CVarManager` and the `Console`, registered in the Mediator at `init` and removed at `shutdown`. Both
  are reachable before `init` through `cvars()` / `console()`.
- Adds the built-in commands at construction.
- Publishes one `tools::CVarChanged{name}` per changed cvar at the next `preUpdate`, before the simulation steps of
  the frame. Code that reads a `CVarRef` every frame does not need the event; it suits code that must react once
  (rebuild a resource, resize a buffer).

Not thread-safe: use the console and the cvars from the main thread.
