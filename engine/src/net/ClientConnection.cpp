#include "ClientConnection.hpp"

#include <vector>

#include "INetworkManager.hpp"

namespace ecs::net {
void ClientConnection::send(PeerId peer, const Bytes& data, Channel ch) {
    if (peer == SERVER_PEER && _client) {
        _client->send(data, ch);
    }
}
void ClientConnection::broadcast(const Bytes& data, Channel ch) {
    if (_client) {
        _client->send(data, ch);
    }
}

void ClientConnection::kick(PeerId /*unused*/) {
    if (_client) {
        _client->stop();
    }
}

std::vector<NetworkEvent> ClientConnection::poll() { return _client ? _client->poll() : std::vector<NetworkEvent>{}; }
}  // namespace ecs::net
