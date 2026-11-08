/**
 * @file IScene.hpp
 * @brief Interface of a scene: a screen or game state that owns entities, event subscriptions and systems.
 */

#pragma once

#include <string_view>

#include "scene/SceneTypes.hpp"

namespace scene {

class SceneContext;

/**
 * @brief A scene of the game: a menu, a level, a pause overlay...
 *
 * A scene is registered once in the `SceneManager` and keeps its state between visits. While it is on the stack,
 * everything it creates through its `SceneContext` (entities, subscriptions, systems) belongs to it and is released
 * when it leaves the stack, so a hook never has to clean up after itself.
 *
 * Hooks run at the frame boundary (`ScenePlugin::preUpdate`), never in the middle of a system, so a scene can spawn and
 * destroy entities freely from them. Every hook is optional.
 */
class IScene {
  public:
    /** @brief Default constructor. */
    IScene() = default;

    /** @brief Virtual destructor. */
    virtual ~IScene() = default;

    /** @brief Deleted copy constructor. */
    IScene(const IScene&) = delete;

    /** @brief Deleted copy assignment. */
    IScene& operator=(const IScene&) = delete;

    /** @brief Deleted move constructor. */
    IScene(IScene&&) = delete;

    /** @brief Deleted move assignment. */
    IScene& operator=(IScene&&) = delete;

    /**
     * @brief Gets the name of the scene, unique in its manager, used to find it by name (scripts, logs).
     * @return Name of the scene.
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * @brief Tells how the scene treats the scenes below it.
     * @return `SceneMode::Exclusive` by default.
     */
    [[nodiscard]] virtual SceneMode mode() const noexcept { return SceneMode::Exclusive; }

    /**
     * @brief For an overlay, tells whether the scenes below stop running while it is on top of them.
     *
     * Ignored for an exclusive scene, which always pauses the scenes below.
     * @return `false` by default: the scenes below an overlay keep running.
     */
    [[nodiscard]] virtual bool pausesBelow() const noexcept { return false; }

    /**
     * @brief Called when the scene is pushed on the stack: spawn its entities, subscribe to events, add systems.
     * @param context Context of the scene.
     */
    virtual void onEnter(SceneContext& context) { (void)context; }

    /**
     * @brief Called when the scene leaves the stack, before its entities, subscriptions and systems are released.
     * @param context Context of the scene.
     */
    virtual void onExit(SceneContext& context) { (void)context; }

    /**
     * @brief Called when a scene pushed above stops this one; its systems no longer run.
     * @param context Context of the scene.
     */
    virtual void onPause(SceneContext& context) { (void)context; }

    /**
     * @brief Called when the scenes pausing this one are gone; its systems run again.
     * @param context Context of the scene.
     */
    virtual void onResume(SceneContext& context) { (void)context; }
};

}  // namespace scene
