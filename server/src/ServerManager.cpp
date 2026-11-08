#include "ServerManager.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include <variant>

#include "Lobby.hpp"
#include "LogTag.hpp"
#include "Logger.hpp"
#include "Room.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace rtype::server {

namespace {

// Sleep between two loop iterations. Short enough to keep the step jitter around 1 ms, long enough not to burn a core.
constexpr auto IDLE_SLEEP = std::chrono::milliseconds(1);

}  // namespace

ServerManager::ServerManager(std::unique_ptr<ecs::net::INetworkServer> network,
                             const std::chrono::milliseconds handshakeTimeout)
    : _network(std::move(network)), _lobby(*_network), _handshakeTimeout(handshakeTimeout) {}

bool ServerManager::run(const std::uint16_t port) {
    if (!_network->start(port)) {
        return false;
    }
    _port = port;
    log::info(LOG_TAG, "listening on port ", port);
    log::debug(LOG_TAG, "debug logs enabled");
    _running = true;

    while (_running.load()) {
        handleEvents();
        expireHandshakes();
        _lobby.update();
        std::this_thread::sleep_for(IDLE_SLEEP);
    }

    // Rooms running in the background send through the network: they stop first.
    _lobby.clear();
    _network->stop();
    log::info(LOG_TAG, "stopped");
    return true;
}

void ServerManager::stop() { _running = false; }

void ServerManager::handleEvents() {
    for (const auto& [type, client, data, channel] : _network->poll()) {
        switch (type) {
            case ecs::net::EventType::Connected:
                onConnected(client);
                break;
            case ecs::net::EventType::Disconnected:
                onDisconnected(client);
                break;
            case ecs::net::EventType::Data:
                onData(client, channel, data);
                break;
        }
    }
}

void ServerManager::onConnected(const ClientId client) {
    _pendingHandshakes[client] = std::chrono::steady_clock::now();
    log::info(LOG_TAG, "client ", client, " connected");
}

void ServerManager::onDisconnected(const ClientId client) {
    _pendingHandshakes.erase(client);
    // Can arrive without onConnected (connection lost before being ready): the lobby then does nothing.
    _lobby.removeClient(client);
    log::info(LOG_TAG, "client ", client, " disconnected");
}

void ServerManager::onData(const ClientId client, const ecs::net::Channel channel, const ecs::net::Bytes& data) {
    if (channel == ecs::net::Channel::Unreliable) {
        onUnreliableData(client, data);
        return;
    }
    const auto message = protocol::decode(data);
    if (!message) {
        log::debug(LOG_TAG, "client ", client, " sent an invalid payload of ", data.size(), " bytes: dropped");
        return;
    }
    onReliableMessage(client, *message);
}

void ServerManager::onReliableMessage(const ClientId client, const protocol::Message& message) {
    if (const auto* hello = std::get_if<protocol::Hello>(&message)) {
        if (_lobby.hasClient(client)) {
            log::debug(LOG_TAG, "client ", client, " sent a second Hello: dropped");
            return;
        }
        if (hello->version != protocol::PROTOCOL_VERSION) {
            log::info(LOG_TAG, "client ", client, " uses protocol version ", hello->version, " (expected ",
                      protocol::PROTOCOL_VERSION, "): refused");
            _pendingHandshakes.erase(client);
            _network->refuse(client, ecs::net::DisconnectReason::ProtocolMismatch);
            return;
        }
        _pendingHandshakes.erase(client);
        _lobby.addClient(client);
        send(client, protocol::Welcome{.udpPort = _port}, ecs::net::Channel::Reliable);
        return;
    }
    if (!_lobby.hasClient(client)) {
        log::debug(LOG_TAG, "client ", client, " sent a message before Hello: dropped");
        return;
    }
    if (std::holds_alternative<protocol::Leave>(message)) {
        _lobby.leaveRoom(client);
        return;
    }
    _lobby.onMessage(client, message);
}

void ServerManager::onUnreliableData(const ClientId client, const ecs::net::Bytes& data) {
    if (!_lobby.hasClient(client)) {
        log::debug(LOG_TAG, "client ", client, " sent UDP data before Hello: dropped");
        return;
    }
    _lobby.onUnreliableData(client, data);
}

void ServerManager::expireHandshakes() {
    const auto now = std::chrono::steady_clock::now();
    // A kicked client leaves the map at once: it is kicked only once, even if its Disconnected event arrives a few
    // iterations later.
    std::erase_if(_pendingHandshakes, [this, now](const auto& pending) {
        const auto& [client, connectedAt] = pending;
        if (now - connectedAt < _handshakeTimeout) {
            return false;
        }
        log::info(LOG_TAG, "client ", client, " sent no Hello within ", _handshakeTimeout.count(), " ms: kicked");
        _network->kick(client);
        return true;
    });
}

void ServerManager::send(const ClientId client, const protocol::Message& message,
                         const ecs::net::Channel channel) const {
    _network->send(client, protocol::encode(message), channel);
}

}  // namespace rtype::server
