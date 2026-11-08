/**
 * @file Console.hpp
 * @brief Headless console interpreter: commands, CVar access, completion, history and output log.
 */

#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tools/console/CommandContext.hpp"  // IWYU pragma: export
#include "tools/console/ConsoleTypes.hpp"    // IWYU pragma: export

namespace tools {

class CVarManager;

/**
 * @brief Headless console interpreter.
 *
 * - `execute` runs a line: statements separated by `;`, each one a command (`help`), a cvar read (`sv_gravity`) or a
 *   cvar change (`sv_gravity 12`). Command names are case-insensitive; a command hides a cvar of the same name.
 * - Every line typed, its results and errors go to the output log, which a UI (or a server terminal) shows.
 * - `complete` suggests command names, cvar names and command arguments for the word being typed.
 * - Executed lines are kept in a history for the UI to browse.
 * - A `raw` command starting the line gets the rest of the line untouched, as one argument (`wren` code...).
 * - `runScript` runs a file of the script root: through the runner registered for its extension (a script engine),
 *   else as console lines (a `.cfg`-style file).
 *
 * Input is untrusted (see `splitCommandLine`); command handlers that throw are reported as errors. Not thread-safe:
 * use it from the main thread.
 */
class Console {
  public:
    /** @brief Most output lines kept; the oldest are dropped first. */
    static constexpr std::size_t MAX_OUTPUT_LINES = 512;
    /** @brief Most history entries kept; the oldest are dropped first. */
    static constexpr std::size_t MAX_HISTORY = 100;
    /** @brief Most scripts running inside each other (`exec` from a script); deeper calls are refused. */
    static constexpr std::size_t MAX_SCRIPT_DEPTH = 8;

    /**
     * @brief Creates a console without commands.
     * @param cvars CVar registry used by the console; must outlive it.
     */
    explicit Console(CVarManager& cvars) noexcept : _cvars(&cvars) {}

    /**
     * @brief Adds a command, or replaces the command of the same name.
     * @param name Name (same rules as a cvar name, see `CVarManager::isValidName`).
     * @param command Description and handler.
     * @throws std::invalid_argument If the name is invalid or the handler is empty.
     */
    void addCommand(std::string name, Command command);

    /**
     * @brief Removes a command.
     * @param name Name.
     * @return `true` if a command was removed.
     */
    bool removeCommand(std::string_view name);

    /**
     * @brief Finds a command.
     * @param name Name.
     * @return The command, or `nullptr`.
     */
    [[nodiscard]] const Command* findCommand(std::string_view name) const noexcept;

    /** @brief Gets the commands. @return Commands sorted by name. */
    [[nodiscard]] const std::map<std::string, Command, std::less<>>& commands() const noexcept { return _commands; }

    /**
     * @brief Runs a line typed by the user, echoing it and recording it in the history.
     * @param line Line, untrusted.
     * @return `true` if every statement ran without error.
     */
    bool execute(std::string_view line);

    /**
     * @brief Runs a line without echoing it or recording it in the history (script files, startup commands).
     * @param line Line, untrusted.
     * @return `true` if every statement ran without error.
     */
    bool run(std::string_view line);

    /**
     * @brief Registers how `runScript` runs the files of an extension, or replaces the runner of that extension.
     * @param extension Extension with its dot, case-insensitive (`.wren`).
     * @param runner Runner.
     * @throws std::invalid_argument If the extension does not start with a dot or the runner is empty.
     */
    void addScriptRunner(std::string_view extension, ScriptRunner runner);

    /**
     * @brief Removes the runner of an extension; its files then run as console lines again.
     * @param extension Extension with its dot.
     * @return `true` if a runner was removed.
     */
    bool removeScriptRunner(std::string_view extension);

    /**
     * @brief Sets the directory script files are read from.
     * @param root Directory; empty for the current directory.
     */
    void setScriptRoot(std::filesystem::path root) { _scriptRoot = std::move(root); }

    /** @brief Gets the script directory. @return Directory script files are read from. */
    [[nodiscard]] const std::filesystem::path& scriptRoot() const noexcept { return _scriptRoot; }

    /**
     * @brief Runs a script file of the script root (see `readScriptFile` for the checks).
     * @param context Context of the command asking for it; errors go through it.
     * @param path Path relative to the script root, untrusted.
     * @return `true` if the script ran without error.
     */
    bool runScript(CommandContext& context, std::string_view path);

    /**
     * @brief Completes the last word of a line.
     * @param line Line being typed.
     * @return The completed line and the candidates; no candidate leaves the line unchanged.
     */
    [[nodiscard]] Completion complete(std::string_view line) const;

    /**
     * @brief Writes a line to the output log.
     * @param level Kind of line.
     * @param text Text; split on newlines.
     */
    void print(OutputLevel level, std::string_view text);

    /** @brief Empties the output log. */
    void clearOutput() noexcept { _output.clear(); }

    /** @brief Gets the output log. @return Lines, oldest first. */
    [[nodiscard]] const std::deque<OutputLine>& output() const noexcept { return _output; }

    /** @brief Gets the history. @return Executed lines, oldest first, without consecutive duplicates. */
    [[nodiscard]] const std::deque<std::string>& history() const noexcept { return _history; }

    /** @brief Gets the CVars. @return The CVar registry. */
    [[nodiscard]] CVarManager& cvars() const noexcept { return *_cvars; }

  private:
    /**
     * @brief Runs one statement.
     * @param statement Name and arguments.
     * @return `true` on success.
     */
    bool runStatement(const std::vector<std::string>& statement);

    /**
     * @brief Runs a line whose first word is a raw command.
     * @param line Valid line.
     * @return The result, or nothing if the line does not start with a raw command.
     */
    std::optional<bool> runRaw(std::string_view line);

    /**
     * @brief Checks the cheat rule and the argument count of a command, then runs it.
     * @param name Command name.
     * @param command Command.
     * @param args Arguments.
     * @return `true` on success.
     */
    bool invoke(const std::string& name, const Command& command, std::span<const std::string> args);

    /**
     * @brief Reads or changes a cvar from a statement.
     * @param name Name of the cvar.
     * @param args Arguments after the name.
     * @return `true` on success.
     */
    bool runCVar(const std::string& name, std::span<const std::string> args);

    /**
     * @brief Records a line in the history.
     * @param line Line typed.
     */
    void remember(std::string_view line);

    /** @brief CVar registry, never null. */
    CVarManager* _cvars;
    /** @brief Commands by name. */
    std::map<std::string, Command, std::less<>> _commands;
    /** @brief Output log. */
    std::deque<OutputLine> _output;
    /** @brief Executed lines. */
    std::deque<std::string> _history;
    /** @brief Script runners by lowercase extension. */
    std::map<std::string, ScriptRunner, std::less<>> _scriptRunners;
    /** @brief Directory script files are read from. */
    std::filesystem::path _scriptRoot;
    /** @brief Scripts currently running inside each other. */
    std::size_t _scriptDepth = 0;
};

/**
 * @brief Describes a cvar on one line: name, value, default, flags and description.
 * @param cvars Registry.
 * @param name Name of the cvar.
 * @return The description, or an empty string if there is no such cvar.
 */
[[nodiscard]] std::string describeCVar(const CVarManager& cvars, std::string_view name);

}  // namespace tools
