/**
 * @file SceneTypes.hpp
 * @brief Small value types shared by the scene module: ids, modes, stages and the membership component.
 */

#pragma once

#include <cstdint>
#include <limits>

namespace scene {

/** @brief Dense identifier of a registered scene, assigned by `SceneManager::registerScene` in registration order. */
using SceneId = std::uint16_t;

/** @brief Id that names no scene, returned by `SceneManager::top()` when the stack is empty. */
inline constexpr SceneId INVALID_SCENE = std::numeric_limits<SceneId>::max();

/** @brief How a scene treats the scenes below it on the stack. */
enum class SceneMode : std::uint8_t {
    /** @brief Covers the scenes below: they are hidden and paused while this scene is on top of them. */
    Exclusive,
    /** @brief Drawn on top of the scenes below, which stay visible; they keep running unless `pausesBelow()`. */
    Overlay,
};

/** @brief Engine stage a scene-local system runs in. */
enum class SceneStage : std::uint8_t {
    /** @brief Every fixed step, with the fixed time (`IPlugin::fixedUpdate`). */
    FixedUpdate,
    /** @brief Once per frame (`IPlugin::update`). */
    Update,
    /** @brief Once per frame, after every plugin's update (`IPlugin::lateUpdate`). */
    LateUpdate,
};

/** @brief Number of `SceneStage` values. */
inline constexpr std::uint8_t SCENE_STAGE_COUNT = 3;

/**
 * @brief Component marking an entity as owned by a scene.
 *
 * Every entity holding it is destroyed when its scene leaves the stack. Entities without it outlive scene changes.
 * Added by `SceneContext::spawn` / `SceneContext::adopt`.
 */
struct SceneMember {
    /** @brief Scene owning the entity. */
    SceneId id = INVALID_SCENE;
};

}  // namespace scene
