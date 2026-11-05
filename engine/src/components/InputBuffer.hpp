#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace ecs {

/**
 * @brief Default maximum number of inputs kept in an `InputBuffer`.
 *
 * Inputs are consumed at the rate they are sent, so a backlog does not shrink on its own: this bounds how late they
 * are applied (about 66 ms with a 60 Hz fixed step).
 */
inline constexpr std::size_t DEFAULT_MAX_PENDING_INPUTS = 4;

/** @brief Value of one axis action during a network input. */
struct NetAxis {
    /** @brief Game-defined axis action id (e.g. an `input::ActionId` such as `makeActionId("move_x")`). */
    std::uint32_t action = 0;
    /** @brief Value of the axis, in [-1, 1]. */
    float value = 0.0F;
};

/**
 * @brief One input received from a remote peer.
 *
 * Actions are opaque ids: their meaning (up, shoot...) is defined by the game, usually `input::ActionId` hashes.
 */
struct NetInput {
    /** @brief Increases with every input sent by the peer; used to drop late inputs and for reconciliation. */
    std::uint32_t sequence = 0;
    /** @brief Game-defined ids of the button actions held during this input. */
    std::vector<std::uint32_t> held;
    /** @brief Axis actions whose value is not 0 during this input. */
    std::vector<NetAxis> axes;
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
