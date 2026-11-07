/**
 * @file ScriptPrefabs.hpp
 * @brief Registers the R-Type prefabs that Wren scripts may spawn on the server, replicated to the clients.
 */

#pragma once

#include "prefabs/PrefabManager.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Registers the prefabs a script may spawn with `Engine.spawnPrefab(name, x, y)`, so that what a boss spawns is
 * seen by every client.
 *
 * Each factory builds the prefab, then adds a `game::EntityNetState` and replicates the entity with the kind
 * `protocol::makeEntityKind(name)` (`docs/protocol/packets.md`, appendix A). Registered: `alien_scout`, `missile`,
 * `power_up`, `obstacle` and `boss_core`. Player ships and player bullets are left out: they belong to a player,
 * which a script cannot name.
 *
 * @param manager Prefab registry handed to the scripting plugin.
 */
void registerScriptPrefabs(prefabs::PrefabManager& manager);

}  // namespace rtype::server::gameplay
