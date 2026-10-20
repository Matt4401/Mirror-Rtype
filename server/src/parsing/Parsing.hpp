#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Wire.hpp"

namespace rtype::server {

/** @brief Flag that turns the debug logs on. */
inline constexpr std::string_view LOGS_FLAG = "--logs";
/** @brief Flag that turns the debug logs on, with colors and bold by severity and origin. */
inline constexpr std::string_view LOGS_COLORFUL_FLAG = "--logs-colorful";

/** @brief Settings of the server, read from the command line. */
struct Options {
    /** @brief TCP and UDP port to listen on. */
    std::uint16_t port = net::wire::DEFAULT_PORT;
    /** @brief Whether the debug logs are printed (`--logs` or `--logs-colorful`). */
    bool logs = false;
    /** @brief Whether the logs are colored (`--logs-colorful`). */
    bool colors = false;
};

/**
 * @brief Parses the command line: `[port] [--logs | --logs-colorful]`, in any order.
 * @param args Every argument of `main()`, program name included.
 * @return The options, or nothing if an argument is unknown, invalid or given twice, or if both log flags are given.
 */
[[nodiscard]] std::optional<Options> parseOptions(std::span<char* const> args);

/**
 * @brief Usage line printed when the command line is invalid.
 * @param programName Name the server was launched with (`argv[0]`).
 */
[[nodiscard]] std::string usage(std::string_view programName);

}  // namespace rtype::server
