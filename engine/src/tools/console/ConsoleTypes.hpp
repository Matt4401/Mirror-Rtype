/**
 * @file ConsoleTypes.hpp
 * @brief Plain types of the console: output lines, command description, completion result, plugin settings.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace tools {

class CommandContext;
class Console;

/** @brief Kind of a line of the console output. */
enum class OutputLevel : std::uint8_t {
    /** @brief A line typed by the user, echoed before its result. */
    Echo,
    /** @brief Normal output of a command. */
    Info,
    /** @brief Error message. */
    Error,
};

/** @brief One line of the console output. */
struct OutputLine {
    /** @brief Kind of line. */
    OutputLevel level;
    /** @brief Text, without newline. */
    std::string text;
};

/** @brief Runs a command. */
using CommandHandler = std::function<void(CommandContext&)>;

/**
 * @brief Suggests values for an argument of a command.
 *
 * Called with the console, the index of the argument being typed (0 for the first argument) and what is typed so far;
 * returns every candidate, the console keeps those starting with the prefix.
 */
using ArgumentCompleter = std::function<std::vector<std::string>(const Console&, std::size_t, std::string_view)>;

/**
 * @brief Description of a console command.
 */
struct Command {
    /** @brief Arguments as shown by `help` (`<cvar> <value>`). */
    std::string usage;
    /** @brief One-line help text. */
    std::string description;
    /** @brief Function run by the command. */
    CommandHandler handler;
    /** @brief Fewest arguments accepted; the console prints the usage below it. */
    std::size_t minArgs = 0;
    /** @brief Most arguments accepted; the console prints the usage above it. */
    std::size_t maxArgs = std::numeric_limits<std::size_t>::max();
    /** @brief Runs only while cheats are allowed (`godmode`, `spawn`...). */
    bool cheat = false;
    /** @brief Suggests argument values for tab completion; may be empty. */
    ArgumentCompleter completer;
};

/** @brief Result of a tab completion. */
struct Completion {
    /** @brief The line completed as far as every candidate agrees (with a trailing space if one candidate is left). */
    std::string line;
    /** @brief Every candidate for the word being typed, sorted; more than one means the user has to choose. */
    std::vector<std::string> candidates;
};

/**
 * @brief Settings of the ConsolePlugin.
 */
struct ConsoleConfig {
    /** @brief Whether cheat cvars and commands are allowed (keep `false` on competitive servers). */
    bool allowCheats = false;
};

}  // namespace tools
