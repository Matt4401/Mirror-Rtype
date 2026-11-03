#include "ClientConnection.hpp"

#include <vector>

#include "INetworkManager.hpp"

namespace engine {
void ClientConnection::send(ecs::net::PeerId peer, const ecs::net::Bytes& data, ecs::net::Channel ch) {
    if (peer == ecs::net::SERVER_PEER) {
        _client->send(data, ch);
    }
}
void ClientConnection::broadcast(const ecs::net::Bytes& data, ecs::net::Channel ch) { _client->send(data, ch); }

void ClientConnection::kick(ecs::net::PeerId /*unused*/) { _client->stop(); }

std::vector<ecs::net::NetworkEvent> ClientConnection::poll() { return _client->poll(); }
}  // namespace engine
