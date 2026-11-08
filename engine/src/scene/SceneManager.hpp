/**
 * @file SceneManager.hpp
 * @brief Registry of the scenes of a game and their stack, with transitions applied at the frame boundary.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <typeindex>
#include <vector>

#include "ecs/Entity.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneTypes.hpp"

namespace engine {
class Engine;
}

namespace ecs::time {
struct Time;
}

namespace scene {

/**
 * @brief Owns the scenes of a game and the stack of the active ones.
 *
 * Scenes are registered once (names are unique), then `switchTo`, `push`, `pop` and `replace` change the stack. These
 * calls only queue the change: `applyTransitions` (run by `ScenePlugin::preUpdate`) applies them in order at the start
 * of the next frame, so no entity disappears in the middle of a system. A transition requested from a hook is applied
 * in the same call.
 *
 * The top scene always runs and is visible. Below it:
 * - an exclusive scene hides and pauses everything under it;
 * - an overlay keeps what is under it visible, and running unless its `pausesBelow()` is `true`.
 *
 * Entities of a hidden scene carry `ecs::Hidden` (reason `Scene`), so the UI and the world renderer skip them.
 *
 * Queries (`top`, `isActive`, `isRunning`, `isVisible`) are flag lookups, cheap enough to be called by any plugin every
 * frame.
 */
class SceneManager {
  public:
    /** @brief Most transitions applied by one `applyTransitions` call, against scenes switching each other forever. */
    static constexpr std::size_t MAX_TRANSITIONS_PER_FRAME = 64;

    /** @brief Creates an empty manager, not attached to an engine yet. */
    SceneManager() = default;

    /** @brief Destructor; does not run the hooks (call `clear()` first, as `ScenePlugin::shutdown` does). */
    ~SceneManager() = default;

    /** @brief Deleted copy constructor. */
    SceneManager(const SceneManager&) = delete;

    /** @brief Deleted copy assignment. */
    SceneManager& operator=(const SceneManager&) = delete;

    /** @brief Deleted move constructor: contexts point to their manager. */
    SceneManager(SceneManager&&) = delete;

    /** @brief Deleted move assignment. */
    SceneManager& operator=(SceneManager&&) = delete;

    /**
     * @brief Constructs and registers a scene.
     * @tparam Scene Concrete scene type.
     * @param args Arguments of its constructor.
     * @return Id of the scene.
     * @throws std::invalid_argument if a scene with the same name is already registered.
     */
    template <typename Scene, typename... Args>
    SceneId registerScene(Args&&... args);

    /**
     * @brief Registers a scene built by the caller.
     * @param scene Scene to own; its dynamic type identifies it for `get<T>()` when it is the only one of its type.
     * @return Id of the scene.
     * @throws std::invalid_argument if `scene` is null, or a scene with the same name is already registered.
     * @throws std::length_error if every id is taken.
     */
    SceneId add(std::unique_ptr<IScene> scene);

    /**
     * @brief Gets a registered scene by type.
     * @tparam Scene Type given to `registerScene`.
     * @return The scene.
     * @throws std::out_of_range if no scene of this type is registered.
     * @throws std::logic_error if several scenes have this type (look them up by name instead).
     */
    template <typename Scene>
    [[nodiscard]] Scene& get();

    /**
     * @brief Gets the id of a registered scene by type.
     * @tparam Scene Type given to `registerScene`.
     * @return Its id.
     * @throws std::out_of_range if no scene of this type is registered.
     * @throws std::logic_error if several scenes have this type (look them up by name instead).
     */
    template <typename Scene>
    [[nodiscard]] SceneId idOf() const;

    /**
     * @brief Finds a scene by name.
     * @param name Name returned by its `IScene::name()`.
     * @return Its id, or nothing if no scene has this name.
     */
    [[nodiscard]] std::optional<SceneId> find(std::string_view name) const noexcept;

    /**
     * @brief Gets a registered scene.
     * @param id Id of the scene.
     * @return The scene.
     * @throws std::out_of_range for an unknown id.
     */
    [[nodiscard]] IScene& scene(SceneId id) const;

    /**
     * @brief Gets the context of a registered scene.
     * @param id Id of the scene.
     * @return Its context.
     * @throws std::out_of_range for an unknown id.
     */
    [[nodiscard]] SceneContext& context(SceneId id) const;

    /**
     * @brief Gets the number of registered scenes.
     * @return Number of scenes.
     */
    [[nodiscard]] std::size_t count() const noexcept { return _slots.size(); }

    /**
     * @brief Queues: exit every scene of the stack (top first), then enter `id`.
     * @param id Scene to enter.
     * @throws std::out_of_range for an unknown id.
     */
    void switchTo(SceneId id);

    /**
     * @brief Queues: enter `id` on top of the stack.
     * @param id Scene to push; it must not be on the stack when the transition is applied.
     * @throws std::out_of_range for an unknown id.
     */
    void push(SceneId id);

    /** @brief Queues: exit the top scene. Does nothing on an empty stack. */
    void pop();

    /**
     * @brief Queues: exit the top scene, then enter `id` in its place.
     * @param id Scene to enter.
     * @throws std::out_of_range for an unknown id.
     */
    void replace(SceneId id);

    /**
     * @brief Queues `switchTo` for a scene given by type.
     * @tparam Scene Registered scene type.
     */
    template <typename Scene>
    void switchTo();

    /**
     * @brief Queues `push` for a scene given by type.
     * @tparam Scene Registered scene type.
     */
    template <typename Scene>
    void push();

    /**
     * @brief Queues `replace` for a scene given by type.
     * @tparam Scene Registered scene type.
     */
    template <typename Scene>
    void replace();

    /**
     * @brief Tells whether transitions wait for the next `applyTransitions`.
     * @return `true` if at least one transition is queued.
     */
    [[nodiscard]] bool hasPendingTransitions() const noexcept { return !_requests.empty(); }

    /**
     * @brief Gets the top scene of the stack.
     * @return Its id, or `INVALID_SCENE` if the stack is empty.
     */
    [[nodiscard]] SceneId top() const noexcept { return _stack.empty() ? INVALID_SCENE : _stack.back(); }

    /**
     * @brief Gets the stack, bottom first.
     * @return Ids of the scenes on the stack.
     */
    [[nodiscard]] std::span<const SceneId> stack() const noexcept { return _stack; }

    /**
     * @brief Gets the scene that owns world content spawned without an explicit owner: the top exclusive scene.
     * @return The highest exclusive scene of the stack, else its bottom scene, or `INVALID_SCENE` if it is empty.
     */
    [[nodiscard]] SceneId spawnTarget() const noexcept;

    /**
     * @brief Tells whether a scene is on the stack.
     * @param id Scene to check; unknown ids are never active.
     * @return `true` if the scene is on the stack.
     */
    [[nodiscard]] bool isActive(SceneId id) const noexcept;

    /**
     * @brief Tells whether a scene runs: on the stack, and not paused by a scene above it.
     * @param id Scene to check; unknown ids never run.
     * @return `true` if its systems run.
     */
    [[nodiscard]] bool isRunning(SceneId id) const noexcept;

    /**
     * @brief Tells whether a scene is visible: on the stack, and not covered by an exclusive scene.
     * @param id Scene to check; unknown ids are never visible.
     * @return `true` if the scene is shown.
     */
    [[nodiscard]] bool isVisible(SceneId id) const noexcept;

    /**
     * @brief Binds the manager to the engine its scenes run in; done by `ScenePlugin::init`.
     * @param engine Host engine.
     */
    void attach(engine::Engine& engine) noexcept { _engine = &engine; }

    /** @brief Unbinds the engine; done by `ScenePlugin::shutdown`. */
    void detach() noexcept { _engine = nullptr; }

    /**
     * @brief Gets the engine the manager is bound to.
     * @return Host engine, or null before `attach`.
     */
    [[nodiscard]] engine::Engine* engine() const noexcept { return _engine; }

    /**
     * @brief Applies the queued transitions, in order, with the transitions their hooks queue.
     *
     * Does nothing before `attach`, nor while `runStage` runs systems: their transitions wait for the next call.
     * @throws std::logic_error if a pushed scene is already on the stack, or if more than
     *         `MAX_TRANSITIONS_PER_FRAME` transitions are applied (scenes switching each other forever).
     */
    void applyTransitions();

    /**
     * @brief Runs the systems of `stage` of every running scene, bottom of the stack first.
     * @param stage Stage to run.
     * @param engine Host engine.
     * @param time Time of the stage.
     */
    void runStage(SceneStage stage, engine::Engine& engine, const ecs::time::Time& time);

    /**
     * @brief Exits every scene now (top first) and drops the queued transitions.
     *
     * Runs the `onExit` hooks; does nothing more than dropping the queue before `attach`. Transitions queued by the
     * exit hooks are dropped too.
     * @throws std::logic_error if called from a scene system (`runStage` is running).
     */
    void clear();

  private:
    /** @brief Kind of a queued transition. */
    enum class Operation : std::uint8_t { Switch, Push, Pop, Replace };

    /** @brief A queued transition. */
    struct Request {
        /** @brief What to do. */
        Operation operation = Operation::Push;
        /** @brief Scene to enter, unused for `Pop`. */
        SceneId id = INVALID_SCENE;
    };

    /** @brief A registered scene and its state. */
    struct Slot {
        /** @brief The scene. */
        std::unique_ptr<IScene> scene;
        /** @brief Dynamic type of the scene, for `get<T>()`. */
        std::type_index type;
        /** @brief What the scene owns. */
        SceneContext context;
        /** @brief On the stack. */
        bool active = false;
        /** @brief Systems run. */
        bool running = false;
        /** @brief Drawn. */
        bool visible = false;
    };

    /**
     * @brief Finds the only scene of a type.
     * @param type Dynamic type of a scene.
     * @return Its id, or nothing if no scene has this type.
     * @throws std::logic_error if several scenes have this type.
     */
    [[nodiscard]] std::optional<SceneId> findType(std::type_index type) const;

    /**
     * @brief Throws for an unknown id.
     * @param id Id to check.
     * @return The slot of `id`.
     */
    [[nodiscard]] Slot& slotOf(SceneId id) const;

    /**
     * @brief Applies one transition.
     * @param request Transition to apply.
     */
    void apply(const Request& request);

    /**
     * @brief Enters a scene on top of the stack and notifies the scenes it pauses or hides.
     * @param id Scene to enter.
     */
    void enter(SceneId id);

    /**
     * @brief Exits the top scene: hook, release of what it owns, destruction of its entities.
     *
     * The scene leaves the stack even if a hook or a destroy listener throws; the first error is rethrown after.
     * @param refresh `true` to notify the scenes below that run or show again.
     */
    void exitTop(bool refresh);

    /**
     * @brief Removes the top scene: releases what it owns, destroys its entities, clears its flags, pops it.
     * @param error Receives the first error thrown by a destroy listener, if it holds none yet.
     */
    void removeTop(std::exception_ptr& error);

    /**
     * @brief Destroys every entity owned by a scene.
     * @param id Scene whose entities to destroy.
     * @return `true` if at least one entity was destroyed.
     */
    bool destroyMembers(SceneId id);

    /**
     * @brief Hides or shows every entity owned by a scene (`ecs::HiddenReason::Scene`).
     * @param id Scene whose entities change.
     * @param hidden `true` to hide them, `false` to show them.
     */
    void setMembersHidden(SceneId id, bool hidden);

    /**
     * @brief Computes which scenes of the stack run and show, and calls the hooks of those that changed.
     * @param silentTop `true` to set the flags of the top scene without notifying it (it is entering).
     */
    void refreshStates(bool silentTop);

    /**
     * @brief Engine the scenes run in; null until `attach`, null again after `detach`.
     *
     * Not owning: the engine owns `ScenePlugin`, which owns the manager, so the engine always outlives it. Every
     * path that reaches the engine is only taken between `attach` and `detach`.
     */
    engine::Engine* _engine = nullptr;
    /** @brief Registered scenes, by id; boxed so contexts keep their address when more scenes are added. */
    std::vector<std::unique_ptr<Slot>> _slots;
    /** @brief Active scenes, bottom first. */
    std::vector<SceneId> _stack;
    /** @brief Queued transitions. */
    std::vector<Request> _requests;
    /** @brief Transitions taken from the queue by the running `applyTransitions`; capacity kept between frames. */
    std::vector<Request> _applying;
    /** @brief Entities of an exiting scene; capacity kept between exits. */
    std::vector<ecs::Entity> _scratch;
    /** @brief `true` while `applyTransitions` runs, so a hook's transitions join the current batch. */
    bool _applyingTransitions = false;
    /** @brief `true` while `runStage` runs systems, which must not be destroyed under it. */
    bool _runningStage = false;
};

}  // namespace scene

#include "scene/SceneManager.tpp"  // IWYU pragma: export
