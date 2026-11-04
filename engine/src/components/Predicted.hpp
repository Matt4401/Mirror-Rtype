#pragma once

#include <cstddef>
#include <deque>

#include "components/InputBuffer.hpp"

namespace ecs {

/** @brief Default maximum number of inputs kept in a `Predicted` history (one second at 60 Hz). */
inline constexpr std::size_t DEFAULT_MAX_PREDICTED_INPUTS = 64;

/**
 * @brief Marks the entity driven by the local player's inputs (client side).
 *
 * Its inputs are applied locally as soon as they are sent, without waiting for the server. When a snapshot arrives,
 * the entity is reset to the server state, then the inputs the server has not processed yet are applied again
 * (reconciliation).
 */
struct Predicted {
    /** @brief Inputs sent but not yet acknowledged by the server, oldest first. */
    std::deque<NetInput> history;
    /** @brief Maximum size of `history`; when exceeded, the oldest input is dropped. */
    std::size_t capacity = DEFAULT_MAX_PREDICTED_INPUTS;
};

}  // namespace ecs
