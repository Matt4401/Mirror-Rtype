#pragma once

#include "INetworkManager.hpp"

// Events published on the engine EventBus by `NetworkPlugin::preUpdate`, one per transport event.
namespace ecs::net {

/** @brief A peer finished the transport handshake: it can now send and receive. */
struct PeerConnected {
    /** @brief The new peer. */
    PeerId peer;
};

/** @brief A peer is gone (closed, crashed, timed out or kicked). */
struct PeerDisconnected {
    /** @brief The peer that left. */
    PeerId peer;
};

/** @brief A peer sent a game message. The bytes are untrusted: validate them before use. */
struct PacketReceived {
    /** @brief Sender; `SERVER_PEER` on a client. */
    PeerId peer;
    /** @brief Channel the message arrived on. */
    Channel channel;
    /** @brief Raw payload, decoded by the game. */
    Bytes data;
};

}  // namespace ecs::net
