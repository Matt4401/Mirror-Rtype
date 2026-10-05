#pragma once
#include "INetworkManager.hpp"
namespace net {

enum class Channel { Reliable, Unreliable };

class INetworkServer : public INetworkManager {
  public:
    virtual bool start(uint16_t tcpPort, uint16_t udpPort) = 0;
    virtual void send(ClientId client, const Bytes& data, Channel channel) = 0;
    virtual void broadcast(const Bytes& data, Channel channel) = 0;
    virtual void kick(ClientId client) = 0;
};
}  // namespace net
