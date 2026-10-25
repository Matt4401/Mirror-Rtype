#pragma once

#include <string_view>

#include "LogLevel.hpp"

/**
 * @brief Logger library (xmake target `log`), usable by the engine, the network module, the server and the client.
 *
 * Each line looks like `[  12.345] LEVEL [tag] message`: the time since the program start in seconds, the severity
 * and the subsystem that logs. Debug and info go to stdout, warnings and errors to stderr. Thread-safe: the network
 * thread and the game thread can log at the same time without mixing their lines.
 */
namespace rtype::log {

/** @brief Turns the debug messages on or off (`--logs`). Thread-safe. */
void setVerbose(bool enabled) noexcept;

/** @brief Whether the debug messages are printed. Thread-safe. */
[[nodiscard]] bool isVerbose() noexcept;

/** @brief Turns the colors (ANSI codes) on or off (`--logs-colorful`). Thread-safe. */
void setColored(bool enabled) noexcept;

/** @brief Whether the messages are colored. Thread-safe. */
[[nodiscard]] bool isColored() noexcept;

/**
 * @brief Logs a message made of all the arguments streamed one after the other.
 *
 * A debug message is dropped before being formatted when the verbose mode is off, so debug logs cost almost nothing
 * on a hot path (e.g. one per received packet).
 *
 * @param level Severity of the message.
 * @param tag Subsystem that logs (e.g. "server", "network").
 * @param args Values printed with `operator<<`.
 */
template <typename... Args>
void message(Level level, std::string_view tag, const Args&... args);

/** @brief Logs a debug message, printed only with `--logs` or `--logs-colorful`. */
template <typename... Args>
void debug(std::string_view tag, const Args&... args);

/** @brief Logs an info message. */
template <typename... Args>
void info(std::string_view tag, const Args&... args);

/** @brief Logs a warning, on stderr. */
template <typename... Args>
void warning(std::string_view tag, const Args&... args);

/** @brief Logs an error, on stderr. */
template <typename... Args>
void error(std::string_view tag, const Args&... args);

namespace detail {

/** @brief Prints one finished line, with or without colors. Called by `message()`. */
void write(Level level, std::string_view tag, std::string_view message);

}  // namespace detail

}  // namespace rtype::log

#include "Logger.tpp"  // IWYU pragma: export
