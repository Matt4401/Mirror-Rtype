#include "net/NetworkFactory.hpp"

#include <memory>
#include <stdexcept>

#include "net/INetworkClient.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkConfig.hpp"
#include "net/asio/NetworkClient.hpp"
#include "net/asio/NetworkServer.hpp"

namespace ecs::net {

std::unique_ptr<INetworkServer> createServer(const NetworkConfig& config) {
    if (!isValid(config)) {
        throw std::invalid_argument("invalid network configuration");
    }
    return std::make_unique<detail::NetworkServer>(config);
}

std::unique_ptr<INetworkClient> createClient(const NetworkConfig& config) {
    if (!isValid(config)) {
        throw std::invalid_argument("invalid network configuration");
    }
    return std::make_unique<detail::NetworkClient>(config);
}

}  // namespace ecs::net
