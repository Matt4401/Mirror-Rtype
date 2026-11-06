#include "net/ClientNetwork.hpp"

#include <memory>

#include "ClientOptions.hpp"
#include "core/Engine.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "protocol/Messages.hpp"

namespace client {

void addNetworkPlugins(engine::Engine& engine, const ClientOptions& options) {
    const std::shared_ptr<ecs::net::INetworkClient> transport =
        ecs::net::createClient({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID});
    const auto connection = std::make_shared<ecs::net::ClientConnection>(transport);

    engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection)
        .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rtype::game::ReplicationCodec>())
        .addPlugin<RTypeNetworkPlugin>(transport)
        .addPlugin<PlayerControlPlugin>();

    transport->connect(options.host, options.port);
}

}  // namespace client
