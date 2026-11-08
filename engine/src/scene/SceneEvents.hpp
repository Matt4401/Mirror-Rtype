/**
 * @file SceneEvents.hpp
 * @brief Events published on the `ecs::EventBus` when scenes change, so plugins can react without knowing them.
 */

#pragma once

#include "scene/SceneTypes.hpp"

namespace scene {

/** @brief A scene was pushed on the stack; published after its `onEnter`. */
struct SceneEntered {
    /** @brief Scene that entered. */
    SceneId id = INVALID_SCENE;
};

/** @brief A scene left the stack; published after its `onExit` and the destruction of its entities. */
struct SceneExited {
    /** @brief Scene that exited. */
    SceneId id = INVALID_SCENE;
};

/** @brief A scene on the stack stopped running because a scene above it pauses it; published after `onPause`. */
struct ScenePaused {
    /** @brief Scene that was paused. */
    SceneId id = INVALID_SCENE;
};

/** @brief A paused scene runs again; published after `onResume`. */
struct SceneResumed {
    /** @brief Scene that was resumed. */
    SceneId id = INVALID_SCENE;
};

/** @brief A scene on the stack was hidden by an exclusive scene above it, or shown again. */
struct SceneVisibilityChanged {
    /** @brief Scene whose visibility changed. */
    SceneId id = INVALID_SCENE;
    /** @brief `true` if the scene is shown again, `false` if it was hidden. */
    bool visible = true;
};

}  // namespace scene
