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

    /** @brief Opaque pointer to the instantiated Wren object handle (`WrenHandle*`). */
    void* instanceHandle = nullptr;

    /** @brief Flag indicating whether this script opts in to the per-frame `update(dt)` loop. */
    bool hasUpdate = false;

    /** @brief Flag set to true once `new(entityId)` and `onSpawn()` have executed. */
    bool initialized = false;
};

}  // namespace ecs
