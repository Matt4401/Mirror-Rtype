#pragma once

#include "ClientOptions.hpp"
#include "core/Engine.hpp"

namespace client {

/**
 * @brief Adds everything the client needs to play online, and starts connecting to the server in the background.
 *
 * Adds `ecs::net::NetworkPlugin`, `ecs::net::ClientReplicationPlugin`, `RTypeNetworkPlugin` and `PlayerControlPlugin`.
 * A failed connection is handled by `RTypeNetworkPlugin` once the engine runs.
 *
 * @param engine Engine of the client.
 * @param options Server to connect to.
 */
void addNetworkPlugins(engine::Engine& engine, const ClientOptions& options);

}  // namespace client
