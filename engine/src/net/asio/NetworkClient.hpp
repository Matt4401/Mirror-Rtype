#pragma once

#include <array>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkConfig.hpp"
#include "net/asio/EventQueue.hpp"
#include "net/asio/TcpSession.hpp"
#include "net/asio/Wire.hpp"

namespace ecs::net::detail {

/** @brief Asio implementation of `INetworkClient`: one network thread, one TCP connection, optional UDP. */
class NetworkClient : public INetworkClient {
  public:
    /** @brief Creates a disconnected client. @param config Valid transport settings. */
    explicit NetworkClient(const NetworkConfig& config);
    /** @brief Disconnects and joins the network thread. */
    ~NetworkClient() override;
    /** @brief Deleted copy constructor: the client owns sockets and a thread. */
    NetworkClient(const NetworkClient&) = delete;
    /** @brief Deleted copy assignment operator. */
    NetworkClient& operator=(const NetworkClient&) = delete;
    /** @brief Deleted move constructor: the network thread holds a pointer to the client. */
    NetworkClient(NetworkClient&&) = delete;
    /** @brief Deleted move assignment operator. */
    NetworkClient& operator=(NetworkClient&&) = delete;

    /** @copydoc INetworkManager::poll */
    std::vector<NetworkEvent> poll() override { return _events.popAll(); }
    /** @copydoc INetworkManager::stop */
    void stop() override;
    /** @copydoc INetworkManager::isRunning */
    [[nodiscard]] bool isRunning() const override { return _state.load() == State::Running; }

    /** @copydoc INetworkClient::connect */
    bool connect(const std::string& host, std::uint16_t port) override;
    /** @copydoc INetworkClient::disconnectReason */
    [[nodiscard]] DisconnectReason disconnectReason() const override { return _disconnectReason.load(); }
    /** @copydoc INetworkClient::openUdp */
    bool openUdp(uint16_t port) override;
    /** @copydoc INetworkClient::closeUdp */
    void closeUdp() override;
    /** @copydoc INetworkClient::send */
    void send(const Bytes& data, Channel channel) override;

  private:
    /**
     * @brief The name of the server is resolved: starts the TCP connection, unless `stop()` was called meanwhile.
     * @param ec Error of the resolution.
     * @param hosts Addresses of the server.
     */
    void onResolved(std::error_code ec, const asio::ip::tcp::resolver::results_type& hosts);
    /**
     * @brief The TCP connection is made: starts the session and sends the transport handshake.
     * @param server Address of the server.
     */
    void onTcpConnected(const asio::ip::address& server);
    /**
     * @brief The server cannot be reached: reports it to the game, unless the attempt was cancelled by `stop()`.
     * @param ec The error.
     */
    void onConnectFailed(std::error_code ec);
    /**
     * @brief Handles a TCP frame from the server.
     * @param kind Kind of the frame.
     * @param payload Payload of the frame (untrusted).
     */
    void onTcpMessage(wire::MessageKind kind, Bytes payload);
    /** @brief Waits for the next UDP datagram. */
    void receiveUdp();
    /**
     * @brief Sends a datagram to the server, with the transport header.
     * @param payload Payload; empty for a keepalive or a bind.
     */
    void sendDatagram(const Bytes& payload);
    /** @brief Schedules the next `onUdpTick()`. */
    void scheduleUdpTick();
    /** @brief Housekeeping: detects the UDP timeout and sends the keepalives and bind datagrams. */
    void onUdpTick();
    /** @brief Closes the UDP socket and stops the housekeeping. */
    void closeUdpNow();

    /** @brief Transport settings. */
    NetworkConfig _config;

    /** @brief Runs every asynchronous operation, on `_thread`. */
    asio::io_context _io;
    /** @brief Keeps `_io` running while no operation is pending. */
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    /** @brief Network thread. */
    std::thread _thread;
    /** @brief Lifecycle state. */
    std::atomic<State> _state{State::Idle};
    /** @brief Why the connection ended. */
    std::atomic<DisconnectReason> _disconnectReason{DisconnectReason::ConnectionLost};

    /** @brief Resolves the name of the server. */
    asio::ip::tcp::resolver _resolver;
    /** @brief Socket being connected, handed to `_tcp` once connected. */
    asio::ip::tcp::socket _connecting;
    /** @brief TCP session; null until the TCP connection is made. Network thread only. */
    std::shared_ptr<TcpSession> _tcp;
    /** @brief Address of the server, also used for UDP. */
    asio::ip::address _serverAddress;
    /** @brief True once the `Welcome` frame was received. */
    bool _welcomed = false;
    /** @brief Id given by the server in `Welcome`, sent in every datagram. */
    PeerId _peerId = 0;
    /** @brief Session token given by the server in `Welcome`, sent in every datagram. */
    std::uint32_t _token = 0;

    /** @brief UDP socket, open between `openUdp()` and `closeUdp()`. */
    asio::ip::udp::socket _udpSocket;
    /** @brief UDP endpoint of the server: datagrams from anywhere else are dropped. */
    asio::ip::udp::endpoint _serverUdp;
    /** @brief Sender of the datagram being received. */
    asio::ip::udp::endpoint _udpFrom;
    /** @brief Datagram being received. */
    std::array<std::uint8_t, 1500> _udpBuffer{};
    /** @brief Timer of `onUdpTick()`. */
    asio::steady_timer _udpTimer;
    /** @brief Last datagram sent, for keepalives. */
    std::chrono::steady_clock::time_point _lastUdpSent;
    /** @brief Last datagram received, for the UDP timeout. */
    std::chrono::steady_clock::time_point _lastUdpReceived;
    /** @brief True once the server answered on UDP: the bind datagrams stop. */
    bool _udpBound = false;

    /** @brief Events waiting for `poll()`. */
    EventQueue _events;
};

}  // namespace ecs::net::detail
