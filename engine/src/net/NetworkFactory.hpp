#pragma once

#include <memory>

#include "net/INetworkClient.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkConfig.hpp"

// Creates the transport implementations without exposing them: game code only includes this header and the
// interfaces, so it never sees Asio. Defined in the "engine_network_asio" target, which must be linked.
namespace ecs::net {

/**
 * @brief Creates the server transport.
 * @param config Transport settings; must satisfy `isValid()`.
 * @return A server, not started yet.
 * @throws std::invalid_argument If `config` is not valid.
 */
[[nodiscard]] std::unique_ptr<INetworkServer> createServer(const NetworkConfig& config = {});

/**
 * @brief Creates the client transport.
 * @param config Transport settings; must satisfy `isValid()`.
 * @return A client, not connected yet.
 * @throws std::invalid_argument If `config` is not valid.
 */
[[nodiscard]] std::unique_ptr<INetworkClient> createClient(const NetworkConfig& config = {});

}  // namespace ecs::net
