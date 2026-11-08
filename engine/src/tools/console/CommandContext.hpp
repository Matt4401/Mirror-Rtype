/**
 * @file CommandContext.hpp
 * @brief What a command handler receives: its arguments, and where to write its output.
 */

#pragma once

#include <span>
#include <string>
#include <string_view>

namespace tools {

class Console;
class CVarManager;

/**
 * @brief What a command handler receives: its arguments, and where to write its output.
 */
class CommandContext {
  public:
    /**
     * @brief Creates a context.
     * @param console Console running the command.
     * @param args Arguments, without the command name.
     */
    CommandContext(Console& console, std::span<const std::string> args) noexcept : _console(&console), _args(args) {}

    /** @brief Gets the arguments. @return Arguments after the command name. */
    [[nodiscard]] std::span<const std::string> args() const noexcept { return _args; }

    /** @brief Gets the console. @return The console running the command. */
    [[nodiscard]] Console& console() const noexcept { return *_console; }

    /** @brief Gets the CVars of the console. @return The CVar registry. */
    [[nodiscard]] CVarManager& cvars() const noexcept;

    /**
     * @brief Writes a line of output.
     * @param text Line.
     */
    void print(std::string_view text) const;

    /**
     * @brief Writes an error line and marks the command as failed.
     * @param text Message.
     */
    void error(std::string_view text);

    /** @brief Tells whether `error` was called. @return `true` if the command failed. */
    [[nodiscard]] bool failed() const noexcept { return _failed; }

  private:
    /** @brief Console running the command, never null. */
    Console* _console;
    /** @brief Arguments after the command name. */
    std::span<const std::string> _args;
    /** @brief Whether the command reported an error. */
    bool _failed = false;
};

}  // namespace tools
