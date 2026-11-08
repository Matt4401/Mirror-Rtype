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

/**
 * @brief Binds `ecs::Transform` to Wren as `Position` (`x`, `y`), in the VM of a room and in every VM a hot reload
 * creates.
 *
 * A script reads and moves its entity with `Position.new(entity.id)`. Reading a dead entity gives nothing and writing
 * it does nothing, so a script running after its boss died cannot crash the room.
 *
 * @param scripts Script system of the room.
 * @return True if the binding was loaded in the room VM.
 */
bool bindScriptComponents(systems::ScriptSystem& scripts);

}  // namespace rtype::server::gameplay
