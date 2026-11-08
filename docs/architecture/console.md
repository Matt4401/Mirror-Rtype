# Console and CVars

Console variables (CVars) are engine and game settings that can be read and changed by name while the game runs:
`sv_gravity 12` changes the gravity on the next physics step, without a restart. The console interpreter runs
commands (`help`, `set`, `spawn alien`...) typed as text.

| Module              | Contents                                                                           | Depends on    |
|---------------------|------------------------------------------------------------------------------------|---------------|
| `engine_tools`      | `CVarManager`, `Console`, `splitCommandLine`, built-in commands, `ConsolePlugin`   | core, events  |
| `engine_tools_scripting` | `ConsoleScriptingPlugin`: the `wren` command and `.wren` files for `exec`     | tools, scripting |
| `engine_devconsole` | `DevConsolePlugin`: the in-game window, toggled with `~` / `²`                      | tools, ui     |

Sources are split by role: `tools/common/` holds shared text helpers, `tools/cvars/` the `CVarManager`,
`tools/console/` the interpreter, the built-in commands and the plugin, `tools/devconsole/` the window.

Neither module uses raylib. `engine_tools` draws nothing, so a server uses it as well as a client. `engine_devconsole`
draws and reads input through the engine services (`IScreenDrawService`, `IWindowService`, `IInputCaptureService`),
so it builds headless and is tested with fake services.

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
| `exec <file>`        | Runs a script file of the script directory (see below)     |

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

## Script files: `exec`

`exec <file>` runs a file of the script directory (`ConsoleConfig::scriptRoot`, the current directory by default):

- The file goes to the **runner registered for its extension** (`Console::addScriptRunner(".wren", runner)`): a
  script engine plugs itself in without the console knowing it.
- Without a runner, the file is **console lines**, run one by one without echo or history (a `.cfg` of settings and
  commands, `//` comments allowed). A failing line is reported and the next ones still run.
- The path is untrusted: absolute paths and paths leaving the script directory (`..`, symbolic links) are refused,
  files larger than 1 MiB are refused, and scripts running each other stop at a depth of 8.

## Running Wren: `wren`

`ConsoleScriptingPlugin` (module `engine_tools_scripting`) joins the console and the script plugin; add it with both:

```cpp
engine.addPlugin<tools::ConsolePlugin>(tools::ConsoleConfig{.allowCheats = true})
    .addPlugin<scripting::ScriptPlugin>()
    .addPlugin<tools::ConsoleScriptingPlugin>();
```

| Line                                     | Effect                                                          |
|------------------------------------------|-----------------------------------------------------------------|
| `wren 1 + 2`                             | An expression prints its value: `=> 3`                          |
| `wren var speed = 3`                     | Defines a variable of the `console` module...                    |
| `wren speed * 2`                         | ...still there on the next line: `=> 6`                         |
| `wren System.print("hi")`                | `System.print` writes to the console (then `=> hi`, its value)  |
| `wren import "core/Engine" for Engine`   | Reaches the engine API through the core modules                 |
| `exec boss_test.wren`                    | Runs a Wren file in a module of its own (each run starts fresh) |

- `wren` is a **raw** command (`Command::raw`): the rest of the line reaches Wren untouched, quotes, `;` and `//`
  included.
- The line is first tried as an expression whose value is printed; if it does not compile as one (a `var`, a
  `class`...), it runs as it is. A runtime error never runs the code a second time.
- While the code runs, `System.print` and the errors go to the console output; the VM handlers are restored after.
- Running code can change anything in the game: `wren` and `.wren` files are **cheats**, refused when cheats are
  disallowed. Both clients allow them: the server stays authoritative.

## ConsolePlugin

- Owns the `CVarManager` and the `Console`, registered in the Mediator at `init` and removed at `shutdown`. Both
  are reachable before `init` through `cvars()` / `console()`.
- Adds the built-in commands at construction, and reads its scripts from `ConsoleConfig::scriptRoot`.
- Publishes one `tools::CVarChanged{name}` per changed cvar at the next `preUpdate`, before the simulation steps of
  the frame. Code that reads a `CVarRef` every frame does not need the event; it suits code that must react once
  (rebuild a resource, resize a buffer).

Not thread-safe: use the console and the cvars from the main thread.

## Developer console window

```cpp
#include "tools/console/ConsolePlugin.hpp"
#include "tools/devconsole/DevConsolePlugin.hpp"

engine.addPlugin<tools::ConsolePlugin>()
    .addPlugin<tools::DevConsolePlugin>();  // after the RenderPlugin, which provides the services it needs
```

Both clients (R-Type and RLSS) add them. The window takes the top half of the screen: the output log (typed lines in blue, errors
in red), a scroll hint, and the input line with a blinking cursor.

| Key                     | Effect                                                   |
|-------------------------|----------------------------------------------------------|
| `~` / `²` (left of `1`) | Opens / closes the console (`DevConsoleConfig::toggleKey`) |
| Escape                  | Closes the console                                       |
| Enter                   | Runs the line                                            |
| Tab                     | Completes the word before the cursor; several candidates are listed in the output |
| Up / Down               | Browses the history; the line being typed comes back after the newest entry |
| Left / Right / Home / End | Moves the cursor                                       |
| Backspace / Delete      | Erases before / after the cursor                         |
| PageUp / PageDown, wheel | Scrolls the output                                      |

Held keys repeat (`repeatDelay`, `repeatInterval`). `DevConsoleConfig` also sets the height, font, sizes and colours.

The plugin is split for testing: `DevConsole` holds the state and editing logic (no device, no drawing), `InputLine`
edits UTF-8 text with a cursor (control characters refused, byte limit enforced), `KeyRepeater` turns held keys into
presses, `DevConsoleView` draws, and `DevConsolePlugin` wires them to the services.

### Blocking the game while the console is open

While the console is open it captures the input through `engine::IInputCaptureService`, provided by the
`RenderPlugin`:

- `IWindowService` then reports no key, mouse button, wheel, gamepad input or typed character. Gameplay
  (`InputSystem`), menus (`UiPlugin`) and any other reader see an idle device without knowing about the console.
- Only the capture owner reads the real input, through `IInputCaptureService`.
- When the capture is released, keys still held stay hidden until released: the Escape that closes the console does
  not also open the pause menu.
- The character typed by the toggle key itself (`²`, `` ` ``) is discarded, so it reaches neither the console nor a
  focused menu text field.

Input is read in `preUpdate`, before the systems of the frame; the window is queued in `lateUpdate`. The plugin
declares `optionally<ui::UiPlugin>()` and returns a low `IPlugin::priority()`, so it runs after the UI and after every
game plugin that does not depend on it (RLSS HUD, name tags...): it stays on top whatever the order the plugins were
added in.
