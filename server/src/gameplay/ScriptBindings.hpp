/**
 * @file ScriptBindings.hpp
 * @brief Exposes the R-Type components that the room scripts (bosses) read and move.
 */

#pragma once

#include "systems/script/ScriptSystem.hpp"

namespace rtype::server::gameplay {

/** @brief Wren module of the position proxy, imported by the scripts as `import "core/Position" for Position`. */
inline constexpr const char* POSITION_MODULE = "core/Position";

/** @brief Wren class of the position proxy. */
inline constexpr const char* POSITION_CLASS = "Position";

/** @brief Wren module of the combat helpers, imported as `import "game/Combat" for Combat`. */
inline constexpr const char* COMBAT_MODULE = "game/Combat";

/** @brief Fastest aimed missile, in pixels per second: `Combat.fireAt` clamps its speed to it. */
inline constexpr double MAX_AIMED_SHOT_SPEED = 900.0;

/** @brief Slowest aimed missile, in pixels per second. */
inline constexpr double MIN_AIMED_SHOT_SPEED = 50.0;

/**
 * @brief Binds the components the boss scripts use, in the VM of a room and in every VM a hot reload creates.
 *
 * - `core/Position`: `ecs::Transform` as `Position` (`x`, `y`).
 * - `game/Combat`: static helpers of the class `Combat`:
 *   - `health(id)`: `[points, max]` of an entity with hit points (a boss, a turret), or null;
 *   - `parts(id)`: ids of the living parts of a boss;
 *   - `partRole(id)`: `"weak_point"`, `"turret"`, `"armor"`, or null for anything else;
 *   - `nearestShip(x, y)`: `[x, y]` of the nearest player ship that can be hit, or null;
 *   - `fireAt(x, y, targetX, targetY, speed)`: spawns a replicated missile at (x, y) flying towards the target, at a
 *     speed clamped to `MIN_AIMED_SHOT_SPEED`–`MAX_AIMED_SHOT_SPEED`.
 *
 * A script reads and moves its entity with `Position.new(entity.id)`. Reading a dead entity gives nothing and writing
 * it does nothing, so a script running after its boss died cannot crash the room.
 *
 * @param scripts Script system of the room.
 * @return True if the binding was loaded in the room VM.
 */
bool bindScriptComponents(systems::ScriptSystem& scripts);

}  // namespace rtype::server::gameplay
