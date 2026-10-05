
#pragma once

#include <cstdint>
#include <vector>

namespace net {

using ClientId = uint32_t;
using Bytes = std::vector<uint8_t>;

enum class EventType { Connected, Disconnected, Data };
using PeerId = uint32_t;

struct NetworkEvent {
    EventType type;
    ClientId client;
    Bytes data;
};

class INetworkManager {
  public:
    virtual ~INetworkManager() = default;
    virtual std::vector<NetworkEvent> poll() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

}  // namespace net
