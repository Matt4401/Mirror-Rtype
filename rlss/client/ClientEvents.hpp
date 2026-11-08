/**
 * @file ClientEvents.hpp
 * @brief Client events published on the engine `EventBus` when the player enters or leaves a match.
 */

#pragma once

#include <cstdint>

namespace rlss::client {

/**
 * @brief Published by `MenuScenesPlugin` when the server accepted the player in a match (`JoinResult`): the snapshots,
 * the `Welcome` and the match statuses that follow belong to this match.
 */
struct MatchJoined {
    /** @brief Id of the match (never 0). */
    std::uint16_t matchId = 0;
};

/**
 * @brief Published by `MenuScenesPlugin` when the player leaves the match, back to the menu. The replicated entities of
 * the match are already removed (each with its `ReplicatedEntityDespawned`).
 */
struct MatchLeft {};

}  // namespace rlss::client
