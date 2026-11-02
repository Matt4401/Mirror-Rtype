#pragma once

#include <optional>

#include "components/InputBuffer.hpp"

namespace ecs::net {

/**
 * @brief Queues an input received over an unreliable channel.
 *
 * Inputs can arrive late, duplicated or out of order. An input already applied (sequence <= `lastProcessed`) or
 * already queued is ignored; the others are inserted sorted by sequence. When the queue exceeds `capacity`, the
 * oldest input is dropped.
 *
 * @param buffer Input buffer of the entity.
 * @param input Received input.
 * @return True if the input was queued.
 */
bool pushInput(ecs::InputBuffer& buffer, const ecs::NetInput& input);

/**
 * @brief Takes the oldest pending input and marks it as processed.
 * @param buffer Input buffer of the entity.
 * @return The input to apply, or nothing if the queue is empty.
 */
std::optional<ecs::NetInput> popInput(ecs::InputBuffer& buffer);

}  // namespace ecs::net
