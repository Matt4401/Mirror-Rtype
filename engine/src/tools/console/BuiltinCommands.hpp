/**
 * @file BuiltinCommands.hpp
 * @brief Commands every console has: help, clear, echo, cvar_list, set, reset, toggle, exec.
 */

#pragma once

namespace tools {

class Console;

/**
 * @brief Adds the built-in commands to a console.
 *
 * - `help [name]`: lists the commands, or describes one command or cvar.
 * - `clear`: empties the output log.
 * - `echo <text...>`: prints its arguments.
 * - `cvar_list [prefix]`: lists the cvars, with value, default, flags and description.
 * - `set <cvar> <value>` / `reset <cvar>`: changes a cvar / restores its default.
 * - `toggle <cvar>`: flips a `bool` cvar.
 * - `exec <file>`: runs a script file (see `Console::runScript`).
 *
 * They replace any command already added under one of these names.
 * @param console Console to fill.
 */
void registerBuiltinCommands(Console& console);

}  // namespace tools
