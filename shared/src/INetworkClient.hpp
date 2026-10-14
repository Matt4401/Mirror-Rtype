#pragma once
#include <cstdint>
#include <string>

#include "INetworkManager.hpp"
namespace net {
class INetworkClient : public INetworkManager {
  public:
    virtual bool connect(const std::string& host, std::uint16_t port) = 0;
    virtual void send(const Bytes& data, Channel channel) = 0;
};
}  // namespace net
