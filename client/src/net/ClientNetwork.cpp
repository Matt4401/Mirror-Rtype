#include "net/ClientNetwork.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/metrics/NetworkMetricsPlugin.hpp"
#include "protocol/Messages.hpp"

namespace client {

void addNetworkPlugins(engine::Engine& engine) {
    const auto connection = std::make_shared<ecs::net::ClientConnection>(nullptr);
    const RTypeNetworkPlugin::TransportFactory factory = [] {
        return ecs::net::createClient({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID});
    };

    engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection)
        .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rtype::game::ReplicationCodec>())
        .addPlugin<RTypeNetworkPlugin>(connection, factory)
        .addPlugin<PlayerControlPlugin>()
        .addPlugin<ecs::net::NetworkMetricsPlugin>([connection] { return connection->stats(); });
}

}  // namespace client
