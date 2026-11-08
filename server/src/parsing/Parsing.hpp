#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "protocol/Messages.hpp"

namespace rtype::server {

/** @brief Flag that turns the debug logs on. */
inline constexpr std::string_view LOGS_FLAG = "--logs";
/** @brief Flag that turns the debug logs on, with colors and bold by severity and origin. */
inline constexpr std::string_view LOGS_COLORFUL_FLAG = "--logs-colorful";

/** @brief Option followed by the path of the level file to play instead of the default one. */
inline constexpr std::string_view LEVEL_OPTION = "--level";

/** @brief Settings of the server, read from the command line. */
struct Options {
    /** @brief TCP and UDP port to listen on. */
    std::uint16_t port = protocol::DEFAULT_PORT;
    /** @brief Whether the debug logs are printed (`--logs` or `--logs-colorful`). */
    bool logs = false;
    /** @brief Whether the logs are colored (`--logs-colorful`). */
    bool colors = false;
    /** @brief Level file given with `--level <path>`; none plays the default level. */
    std::optional<std::string> level;
};

/**
 * @brief Parses the command line: `[port] [--logs | --logs-colorful] [--level <path>]`, in any order.
 * @param args Every argument of `main()`, program name included.
 * @return The options, or nothing if an argument is unknown, invalid or given twice, if both log flags are given, or
 * if `--level` has no path.
 */
[[nodiscard]] std::optional<Options> parseOptions(std::span<char* const> args);

/**
 * @brief Usage line printed when the command line is invalid.
 * @param programName Name the server was launched with (`argv[0]`).
 */
[[nodiscard]] std::string usage(std::string_view programName);

}  // namespace rtype::server
