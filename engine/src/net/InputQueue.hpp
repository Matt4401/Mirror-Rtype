#pragma once

#include <optional>

#include "components/InputBuffer.hpp"

namespace ecs::net {

/**
 * @brief Queues an input received over an unreliable channel.
 *
 * Inputs can arrive late, duplicated or out of order. An input already applied (sequence <= `lastProcessed`), already
 * queued, or older than every input of a full queue is ignored; the others are inserted sorted by sequence. When the
 * queue exceeds `capacity`, the oldest input is dropped.
 *
 * @param buffer Input buffer of the entity.
 * @param input Received input, moved into the queue.
 * @return True if the input is now in the queue.
 */
bool pushInput(ecs::InputBuffer& buffer, ecs::NetInput input);

/**
 * @brief Takes the oldest pending input and marks it as processed.
 * @param buffer Input buffer of the entity.
 * @return The input to apply, or nothing if the queue is empty.
 */
std::optional<ecs::NetInput> popInput(ecs::InputBuffer& buffer);

}  // namespace ecs::net
