#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

namespace ecs {

/** @brief Default maximum number of inputs kept in an `InputBuffer`. */
inline constexpr std::size_t DEFAULT_MAX_PENDING_INPUTS = 8;

/**
 * @brief One input received from a remote peer.
 *
 * `actions` is an opaque bitmask: its meaning (up, shoot...) is defined by the game.
 */
struct NetInput {
    /** @brief Increases with every input sent by the peer; used to drop late inputs and for reconciliation. */
    std::uint32_t sequence = 0;
    /** @brief Game-defined bitmask of the actions held during this input. */
    std::uint16_t actions = 0;
};

/**
 * @brief Inputs of a remote peer not applied yet, for the entity it controls.
 *
 * Plain data: use `ecs::net::pushInput()` / `ecs::net::popInput()` to keep the queue sorted, deduplicated and bounded.
 */
struct InputBuffer {
    /** @brief Inputs not applied yet, sorted by sequence (oldest first). */
    std::deque<NetInput> pending;
    /** @brief Sequence number of the last applied input (0 if none), sent back for client reconciliation. */
    std::uint32_t lastProcessed = 0;
    /** @brief Maximum number of pending inputs; when exceeded, the oldest one is dropped. */
    std::size_t capacity = DEFAULT_MAX_PENDING_INPUTS;
};

}  // namespace ecs
