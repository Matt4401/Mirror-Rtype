#pragma once

#include "core/Engine.hpp"

namespace client {

/**
 * @brief Adds everything the client needs to play online; nothing connects until a `ConnectRequest` is published.
 *
 * Adds `ecs::net::NetworkPlugin`, `ecs::net::ClientReplicationPlugin`, `RTypeNetworkPlugin` (with a factory making a
 * fresh asio transport for each connection) and `PlayerControlPlugin`.
 *
 * @param engine Engine of the client.
 */
void addNetworkPlugins(engine::Engine& engine);

}  // namespace client
