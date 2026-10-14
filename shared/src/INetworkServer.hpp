#pragma once
#include <cstdint>

#include "INetworkManager.hpp"
namespace net {

class INetworkServer : public INetworkManager {
  public:
    virtual bool start(std::uint16_t tcpPort, std::uint16_t udpPort) = 0;
    virtual void send(ClientId client, const Bytes& data, Channel channel) = 0;
    virtual void broadcast(const Bytes& data, Channel channel) = 0;
    virtual void kick(ClientId client) = 0;
};
}  // namespace net
