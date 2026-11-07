#include "ServerConnection.hpp"

#include <vector>

#include "net/INetworkManager.hpp"

namespace rlss::server {

void ServerConnection::send(const ecs::net::PeerId peer, const ecs::net::Bytes& data, const ecs::net::Channel channel) {
    _server->send(peer, data, channel);
}

void ServerConnection::broadcast(const ecs::net::Bytes& data, const ecs::net::Channel channel) {
    _server->broadcast(data, channel);
}

void ServerConnection::kick(const ecs::net::PeerId peer) { _server->kick(peer); }

std::vector<ecs::net::NetworkEvent> ServerConnection::poll() { return _server->poll(); }

}  // namespace rlss::server
