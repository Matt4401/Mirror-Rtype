#include "PeerGroupConnection.hpp"

#include <mutex>
#include <utility>
#include <vector>

#include "INetworkManager.hpp"

namespace ecs::net {

void PeerGroupConnection::addPeer(const PeerId peer) {
    const std::scoped_lock lock(_mutex);
    _peers.insert(peer);
}

void PeerGroupConnection::removePeer(const PeerId peer) {
    const std::scoped_lock lock(_mutex);
    _peers.erase(peer);
}

void PeerGroupConnection::push(NetworkEvent event) {
    const std::scoped_lock lock(_mutex);
    if (_peers.contains(event.peer)) {
        _events.push_back(std::move(event));
    }
}

std::vector<NetworkEvent> PeerGroupConnection::poll() {
    const std::scoped_lock lock(_mutex);
    return std::exchange(_events, {});
}

void PeerGroupConnection::send(const PeerId peer, const Bytes& data, const Channel channel) {
    const std::scoped_lock lock(_mutex);
    if (_peers.contains(peer)) {
        _server->send(peer, data, channel);
    }
}

void PeerGroupConnection::broadcast(const Bytes& data, const Channel channel) {
    const std::scoped_lock lock(_mutex);
    for (const PeerId peer : _peers) {
        _server->send(peer, data, channel);
    }
}

void PeerGroupConnection::kick(const PeerId peer) {
    const std::scoped_lock lock(_mutex);
    if (_peers.contains(peer)) {
        _server->kick(peer);
    }
}

}  // namespace ecs::net
