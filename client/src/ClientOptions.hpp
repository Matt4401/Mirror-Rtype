#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "protocol/Messages.hpp"

namespace client {

/** @brief What the player gives on the command line. */
struct ClientOptions {
    /** @brief Server name or address. */
    std::string host = "127.0.0.1";
    /** @brief Server TCP port. */
    std::uint16_t port = rtype::protocol::DEFAULT_PORT;
};

/**
 * @brief Reads `[host] [port]`.
 * @param args Arguments of `main()`, the name of the program included.
 * @return The options, or nothing if the port is invalid or there are too many arguments.
 */
[[nodiscard]] std::optional<ClientOptions> parseOptions(std::span<char* const> args);

}  // namespace client
