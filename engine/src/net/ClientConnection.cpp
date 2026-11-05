#include "ClientConnection.hpp"

#include <vector>

#include "INetworkManager.hpp"

namespace ecs::net {
void ClientConnection::send(PeerId peer, const Bytes& data, Channel ch) {
    if (peer == SERVER_PEER) {
        _client->send(data, ch);
    }
}
void ClientConnection::broadcast(const Bytes& data, Channel ch) { _client->send(data, ch); }

void ClientConnection::kick(PeerId /*unused*/) { _client->stop(); }

std::vector<NetworkEvent> ClientConnection::poll() { return _client->poll(); }
}  // namespace ecs::net
