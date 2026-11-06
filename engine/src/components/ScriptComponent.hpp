/**
 * @file ScriptComponent.hpp
 * @brief ECS component binding a Wren script class instance to an entity.
 */

#pragma once
#include <string>

namespace ecs {

/**
 * @brief Component attaching a Wren script class instance to an ECS entity.
 *
 * Scripters define a class in a `.wren` module (e.g. `class PlayerScript`).
 * When this component is attached, `ScriptSystem` handles lifecycle instantiation,
 * event reception, and coroutines.
 */
struct ScriptComponent {
    /** @brief Wren module identifier containing the script (e.g. "gameplay/player"). */
    std::string moduleName;

    /** @brief Name of the class defined in the module (e.g. "PlayerScript"). */
    std::string className;

    /** @brief Flag indicating whether this script opts in to the per-frame `update(dt)` loop. */
    bool hasUpdate = false;

    /**
     * @brief Flag indicating whether this script opts in to `onCollision(other)`.
     *
     * When set, the script must define `onCollision(other)`: it is called with the other `Entity` each time a
     * collision begins (`events::CollisionBeginEvent`). Wren cannot check if a method exists, hence the opt-in.
     */
    bool hasCollision = false;

    /** @brief Flag set to true once `new(entityId)` and `onSpawn()` have executed. */
    bool initialized = false;
};

}  // namespace ecs
