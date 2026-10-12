#pragma once

#include <array>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "EventQueue.hpp"
#include "Network/INetworkClient.hpp"
#include "Network/INetworkManager.hpp"
#include "Wire.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::client {

class NetworkClient : public INetworkClient {
  public:
    explicit NetworkClient();
    ~NetworkClient() override;
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) = delete;
    NetworkClient& operator=(NetworkClient&&) = delete;

    std::vector<NetworkEvent> poll() override { return _events.popAll(); }
    void stop() override;
    [[nodiscard]] bool isRunning() const override { return _state.load() == State::Running; }

    bool connect(const std::string& host, std::uint16_t port) override;
    bool openUdp(uint16_t port) override;
    void closeUdp() override;
    void send(const Bytes& data, Channel channel) override;

  private:
    void onTcpMessage(wire::MessageKind kind, Bytes payload);
    void receiveUdp();
    void sendDatagram(const Bytes& payload);
    void scheduleUdpTick();
    void onUdpTick();
    void closeUdpNow();

    asio::io_context _io;
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    std::thread _thread;
    std::atomic<State> _state{State::Idle};

    std::shared_ptr<session::TcpSession> _tcp;
    asio::ip::address _serverAddress;
    bool _welcomed = false;
    ClientId _clientId = 0;
    std::uint32_t _token = 0;

    asio::ip::udp::socket _udpSocket;
    asio::ip::udp::endpoint _serverUdp;
    asio::ip::udp::endpoint _udpFrom;
    std::array<std::uint8_t, 1500> _udpBuffer{};
    asio::steady_timer _udpTimer;
    std::chrono::steady_clock::time_point _lastUdpSent;
    std::chrono::steady_clock::time_point _lastUdpReceived;
    bool _udpBound = false;

    queue::EventQueue _events;
};

}  // namespace rtype::net::client
