/**
 * @file Console.hpp
 * @brief Headless console interpreter: commands, CVar access, completion, history and output log.
 */

#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
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
    bool run(const std::vector<std::string>& statement);

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
};

/**
 * @brief Describes a cvar on one line: name, value, default, flags and description.
 * @param cvars Registry.
 * @param name Name of the cvar.
 * @return The description, or an empty string if there is no such cvar.
 */
[[nodiscard]] std::string describeCVar(const CVarManager& cvars, std::string_view name);

}  // namespace tools
