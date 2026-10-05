#pragma once
#include <string>

#include "INetworkManager.hpp"
namespace net {
class INetworkClient : public INetworkManager {
  public:
    virtual bool connect(const std::string& host, uint16_t port) = 0;
    virtual void send(const Bytes& data) = 0;
};
}  // namespace net
