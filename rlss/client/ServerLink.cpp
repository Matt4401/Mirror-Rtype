#include "ServerLink.hpp"

#include <chrono>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"

namespace rlss::client {

namespace {

constexpr std::chrono::milliseconds POLL_INTERVAL{10};

}  // namespace

void ServerLink::send(const ecs::net::PeerId peer, const ecs::net::Bytes& data, const ecs::net::Channel channel) {
    if (peer == ecs::net::SERVER_PEER) {
        _client->send(data, channel);
    }
}

void ServerLink::broadcast(const ecs::net::Bytes& data, const ecs::net::Channel channel) {
    _client->send(data, channel);
}

void ServerLink::kick(const ecs::net::PeerId /*peer*/) { _client->stop(); }

std::vector<ecs::net::NetworkEvent> ServerLink::poll() {
    std::vector<ecs::net::NetworkEvent> events = std::exchange(_pending, {});
    std::vector<ecs::net::NetworkEvent> received = _client->poll();
    events.insert(events.end(), std::make_move_iterator(received.begin()), std::make_move_iterator(received.end()));
    return events;
}

std::optional<std::vector<ecs::net::NetworkEvent>> connectToServer(ecs::net::INetworkClient& client,
                                                                   const std::string& host, const std::uint16_t port,
                                                                   const std::chrono::milliseconds timeout) {
    if (!client.connect(host, port)) {
        return std::nullopt;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<ecs::net::NetworkEvent> events;
    bool connected = false;
    while (std::chrono::steady_clock::now() < deadline) {
        for (ecs::net::NetworkEvent& event : client.poll()) {
            if (event.type == ecs::net::EventType::Disconnected && !connected) {
                return std::nullopt;
            }
            connected = connected || event.type == ecs::net::EventType::Connected;
            if (connected) {
                events.push_back(std::move(event));
            }
        }
        if (connected) {
            return events;
        }
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
    client.stop();
    return std::nullopt;
}

const char* describe(const ecs::net::DisconnectReason reason) noexcept {
    switch (reason) {
        case ecs::net::DisconnectReason::ProtocolMismatch:
            return "the server runs another game or version";
        case ecs::net::DisconnectReason::ServerFull:
            return "the server is full";
        case ecs::net::DisconnectReason::Unreachable:
            return "the server cannot be reached";
        case ecs::net::DisconnectReason::ConnectionLost:
            break;
    }
    return "connection to the server lost";
}

}  // namespace rlss::client
