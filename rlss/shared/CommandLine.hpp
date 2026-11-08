/**
 * @file CommandLine.hpp
 * @brief Command-line helpers shared by the RLSS executables.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace rlss {

/**
 * @brief Reads a port number between 1 and 65535.
 * @param arg Command-line argument.
 * @return The port, or nothing if `arg` is not a valid port.
 */
[[nodiscard]] std::optional<std::uint16_t> parsePort(std::string_view arg);

}  // namespace rlss
