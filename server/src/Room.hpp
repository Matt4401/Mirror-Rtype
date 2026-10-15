#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"

namespace rtype::server {

using net::ClientId;

class Room {
  public:
    explicit Room(std::uint8_t maxPlayers);

    bool addPlayer(ClientId player);
    void removePlayer(ClientId player);
    void onInput(ClientId player /*, Input décodé */);
    void update();
    // WorldState snapshot() const;
    [[nodiscard]] const std::vector<ClientId>& players() const;
    bool start();

  private:
    std::vector<ClientId> _players;
    std::uint8_t _maxPlayers;
    // WorldState _worldState; Todo: implement WorldState with the protocol ALEXIS ;)
};

}  // namespace rtype::server
