#pragma once

#include <array>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkConfig.hpp"
#include "net/asio/EventQueue.hpp"
#include "net/asio/TcpSession.hpp"
#include "net/asio/Wire.hpp"

namespace ecs::net::detail {

/** @brief Transport state of one connection, owned by the network thread. */
struct PeerInfo {
    /** @brief Id given at accept time. */
    PeerId id = 0;
    /** @brief Random secret sent in `Welcome`, required in every client datagram. */
    std::uint32_t token = 0;
    /** @brief TCP connection. */
    std::shared_ptr<TcpSession> tcp;
    /** @brief True once a valid `Handshake` was received: only then does the game know this peer. */
    bool handshaken = false;
    /** @brief When the TCP connection was accepted, for the handshake timeout. */
    std::chrono::steady_clock::time_point acceptedAt;
    /** @brief True between `UdpOpen` and `UdpClose`. */
    bool udpOpen = false;
    /** @brief Source endpoint of the last valid datagram. */
    std::optional<asio::ip::udp::endpoint> udp;
    /** @brief Last valid datagram received, for the UDP timeout. */
    std::chrono::steady_clock::time_point lastSeen;
    /** @brief Last datagram sent, for keepalives. */
    std::chrono::steady_clock::time_point lastSent;
};

/** @brief Asio implementation of `INetworkServer`: one network thread, TCP + UDP on the same port. */
class NetworkServer : public INetworkServer {
  public:
    /** @brief Creates a stopped server. @param config Valid transport settings. */
    explicit NetworkServer(const NetworkConfig& config);
    /** @brief Stops the server and joins its thread. */
    ~NetworkServer() override;
    /** @brief Deleted copy constructor: the server owns sockets and a thread. */
    NetworkServer(const NetworkServer&) = delete;
    /** @brief Deleted copy assignment operator. */
    NetworkServer& operator=(const NetworkServer&) = delete;
    /** @brief Deleted move constructor: the network thread holds a pointer to the server. */
    NetworkServer(NetworkServer&&) = delete;
    /** @brief Deleted move assignment operator. */
    NetworkServer& operator=(NetworkServer&&) = delete;

    /** @copydoc INetworkManager::poll */
    std::vector<NetworkEvent> poll() override { return _events.popAll(); }
    /** @copydoc INetworkManager::stop */
    void stop() override;
    /** @copydoc INetworkManager::isRunning */
    [[nodiscard]] bool isRunning() const override { return _state.load() == State::Running; }

    /** @copydoc INetworkServer::start */
    bool start(std::uint16_t port) override;
    /** @copydoc INetworkServer::send */
    void send(PeerId peer, const Bytes& data, Channel channel) override;
    /** @copydoc INetworkServer::broadcast */
    void broadcast(const Bytes& data, Channel channel) override;
    /** @copydoc INetworkServer::kick */
    void kick(PeerId peer) override;
    /** @copydoc INetworkServer::refuse */
    void refuse(PeerId peer, DisconnectReason reason) override;

  private:
    /** @brief Waits for the next TCP connection. */
    void accept();
    /**
     * @brief Tells a new connection the server is full and closes it. It never becomes a peer.
     * @param socket The connection.
     */
    void refuseConnection(asio::ip::tcp::socket socket);
    /**
     * @brief Handles a TCP frame: the handshake first, then game data and the UDP open / close requests.
     * @param peer Sender.
     * @param kind Kind of the frame.
     * @param payload Payload of the frame (untrusted).
     */
    void onTcpMessage(PeerId peer, wire::MessageKind kind, Bytes payload);
    /**
     * @brief Checks the `Handshake` of a peer: it is either announced to the game and welcomed, or refused.
     * @param info The peer.
     * @param payload Payload of the frame (untrusted).
     */
    void onHandshake(PeerInfo& info, const Bytes& payload);
    /**
     * @brief Forgets a peer whose connection is closed, and tells the game if it knew this peer.
     * @param peer The peer.
     */
    void onTcpClosed(PeerId peer);

    /** @brief Waits for the next UDP datagram. */
    void receiveUdp();
    /**
     * @brief Validates the datagram in `_udpBuffer`, records the endpoint of its sender and hands its payload to the
     * game.
     * @param size Size of the datagram.
     */
    void handleDatagram(std::size_t size);
    /**
     * @brief Sends a message from the network thread; dropped if the peer is unknown or cannot receive it yet.
     * @param peer Destination peer.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void sendNow(PeerId peer, const Bytes& data, Channel channel);
    /**
     * @brief Sends a datagram to the bound endpoint of a peer; dropped if it has none yet.
     * @param peer Destination peer.
     * @param datagram Datagram; empty for a keepalive.
     */
    void sendDatagram(PeerInfo& peer, Bytes datagram);

    /** @brief Schedules the next `onUdpTick()`. */
    void scheduleUdpTick();
    /** @brief Housekeeping: closes the peers that timed out and sends the keepalives. */
    void onUdpTick();

    /** @brief Transport settings. */
    NetworkConfig _config;

    /** @brief Runs every asynchronous operation, on `_thread`. */
    asio::io_context _io;
    /** @brief Keeps `_io` running while no operation is pending. */
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    /** @brief Network thread. */
    std::thread _thread;

    /** @brief Accepts the TCP connections. */
    asio::ip::tcp::acceptor _acceptor;

    /** @brief UDP socket shared by every peer. */
    asio::ip::udp::socket _udpSocket;
    /** @brief Sender of the datagram being received. */
    asio::ip::udp::endpoint _udpFrom;
    /** @brief Datagram being received; larger than `MAX_DATAGRAM` so that an oversized one can be detected. */
    std::array<std::uint8_t, 1500> _udpBuffer{};

    /** @brief Timer of `onUdpTick()`. */
    asio::steady_timer _udpTimer;

    /** @brief Open connections, including those still handshaking. Network thread only. */
    std::unordered_map<PeerId, PeerInfo> _peers;
    /** @brief Id given to the next connection. */
    PeerId _nextId = 1;
    /** @brief Source of the session tokens. */
    std::random_device _random;

    /** @brief Events waiting for `poll()`. */
    EventQueue _events;
    /** @brief Lifecycle state. */
    std::atomic<State> _state{State::Idle};
};

}  // namespace ecs::net::detail
