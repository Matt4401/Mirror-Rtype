#include "PeerGroupConnection.hpp"

#include <mutex>
#include <utility>
#include <vector>

#include "INetworkManager.hpp"

namespace engine {

void PeerGroupConnection::addPeer(const ecs::net::PeerId peer) {
    const std::scoped_lock lock(_mutex);
    _peers.insert(peer);
}

void PeerGroupConnection::removePeer(const ecs::net::PeerId peer) {
    const std::scoped_lock lock(_mutex);
    _peers.erase(peer);
}

void PeerGroupConnection::push(ecs::net::NetworkEvent event) {
    const std::scoped_lock lock(_mutex);
    if (_peers.contains(event.peer)) {
        _events.push_back(std::move(event));
    }
}

std::vector<ecs::net::NetworkEvent> PeerGroupConnection::poll() {
    const std::scoped_lock lock(_mutex);
    return std::exchange(_events, {});
}

void PeerGroupConnection::send(const ecs::net::PeerId peer, const ecs::net::Bytes& data,
                               const ecs::net::Channel channel) {
    const std::scoped_lock lock(_mutex);
    if (_peers.contains(peer)) {
        _server->send(peer, data, channel);
    }
}

void PeerGroupConnection::broadcast(const ecs::net::Bytes& data, const ecs::net::Channel channel) {
    const std::scoped_lock lock(_mutex);
    for (const ecs::net::PeerId peer : _peers) {
        _server->send(peer, data, channel);
    }
}

void PeerGroupConnection::kick(const ecs::net::PeerId peer) {
    const std::scoped_lock lock(_mutex);
    if (_peers.contains(peer)) {
        _server->kick(peer);
    }
}

}  // namespace engine
