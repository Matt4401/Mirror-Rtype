#pragma once

#include <cstdint>

namespace client {

/** @brief The game has started and UDP is open. Published by `RTypeNetworkPlugin`. */
struct GameStarted {
    /** @brief Index of the ship this client controls. */
    std::uint8_t playerIndex = 0;
};

}  // namespace client
