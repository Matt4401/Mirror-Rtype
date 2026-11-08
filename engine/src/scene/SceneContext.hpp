/**
 * @file SceneContext.hpp
 * @brief Handle a scene uses to create what it owns: entities, event subscriptions and scene-local systems.
 */

#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "scene/SceneTypes.hpp"
#include "utils/MoveOnlyFunction.hpp"

namespace engine {
class Engine;
}

namespace ecs {
class Registry;
}

namespace ecs::time {
struct Time;
}

namespace scene {

class SceneManager;

/** @brief Scene-local system: called with the engine and the time of its stage, only while its scene runs. */
using SceneSystem = ecs::MoveOnlyFunction<void(engine::Engine&, const ecs::time::Time&)>;

/**
 * @brief What a scene owns while it is on the stack.
 *
 * One context per registered scene, owned by the `SceneManager`. Everything created through it is released when the
 * scene exits: entities spawned or adopted are destroyed, subscriptions are cancelled and systems are dropped.
 *
 * Storage keeps its capacity between visits, so entering a scene again does not allocate for its bookkeeping.
 */
class SceneContext {
  public:
    /**
     * @brief Creates the context of a scene.
     * @param manager Manager owning the scene.
     * @param id Id of the scene.
     */
    SceneContext(SceneManager& manager, SceneId id) noexcept : _manager(&manager), _id(id) {}

    /** @brief Destructor. */
    ~SceneContext() = default;

    /** @brief Deleted copy constructor. */
    SceneContext(const SceneContext&) = delete;

    /** @brief Deleted copy assignment. */
    SceneContext& operator=(const SceneContext&) = delete;

    /** @brief Move constructor, used while the manager's storage grows. */
    SceneContext(SceneContext&&) noexcept = default;

    /** @brief Move assignment. */
    SceneContext& operator=(SceneContext&&) noexcept = default;

    /**
     * @brief Gets the id of the scene.
     * @return Id given at registration.
     */
    [[nodiscard]] SceneId id() const noexcept { return _id; }

    /**
     * @brief Gets the manager of the scene.
     * @return Owning manager.
     */
    [[nodiscard]] SceneManager& manager() const noexcept { return *_manager; }

    /**
     * @brief Gets the engine the scene runs in.
     * @return Host engine.
     * @throws std::logic_error before the `ScenePlugin` is initialized.
     */
    [[nodiscard]] engine::Engine& engine() const;

    /**
     * @brief Gets the registry of the engine.
     * @return Registry of the host engine.
     */
    [[nodiscard]] ecs::Registry& registry() const;

    /**
     * @brief Gets the event bus of the engine.
     * @return Event bus of the host engine.
     */
    [[nodiscard]] ecs::EventBus& bus() const;

    /**
     * @brief Tells whether the scene's systems run (it is on the stack and nothing above pauses it).
     * @return `true` while the scene runs.
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Tells whether the scene is drawn (it is on the stack and no exclusive scene covers it).
     * @return `true` while the scene is visible.
     */
    [[nodiscard]] bool isVisible() const noexcept;

    /**
     * @brief Spawns an entity owned by the scene.
     * @return New entity, holding a `SceneMember` for this scene.
     */
    ecs::Entity spawn();

    /**
     * @brief Gives an existing entity to the scene: it is destroyed when the scene exits.
     *
     * Replaces the previous owner, if any. Does nothing for a dead entity.
     * @param entity Entity to own.
     */
    void adopt(ecs::Entity entity);

    /**
     * @brief Subscribes to an event for as long as the scene is on the stack.
     *
     * The subscription also delivers events while the scene is paused; check `isRunning()` if that matters.
     * @tparam EventType Event to listen to.
     * @param callback Listener.
     */
    template <typename EventType>
    void subscribe(ecs::MoveOnlyFunction<void(const EventType&)> callback);

    /**
     * @brief Adds a system run at `stage` while the scene runs, until it exits.
     *
     * A system added while systems run (from a system or an event of that frame) starts at the next call of its
     * stage, so the running list never changes under its own loop.
     * @param stage Stage to run in.
     * @param system System to run.
     */
    void addSystem(SceneStage stage, SceneSystem system);

    /**
     * @brief Runs the systems of `stage`.
     * @param stage Stage to run.
     * @param engine Host engine.
     * @param time Time of the stage.
     */
    void runSystems(SceneStage stage, engine::Engine& engine, const ecs::time::Time& time);

    /**
     * @brief Cancels the subscriptions and drops the systems; called by the manager when the scene exits.
     */
    void release();

  private:
    /** @brief Cancels one subscription of a known event type. */
    using Unsubscriber = void (*)(ecs::EventBus&, ecs::EventBus::EventToken);

    /** @brief A subscription to cancel on exit. */
    struct Subscription {
        /** @brief Token returned by the bus. */
        ecs::EventBus::EventToken token = 0;
        /** @brief Typed unsubscribe call, without allocation. */
        Unsubscriber unsubscribe = nullptr;
    };

    /** @brief A system added while systems were running, merged before the next run of its stage. */
    struct PendingSystem {
        /** @brief Stage of the system. */
        SceneStage stage = SceneStage::Update;
        /** @brief The system. */
        SceneSystem system;
    };

    /**
     * @brief Typed unsubscribe call stored in `Subscription`.
     * @tparam EventType Event of the subscription.
     * @param bus Bus to unsubscribe from.
     * @param token Token of the subscription.
     */
    template <typename EventType>
    static void unsubscribeFrom(ecs::EventBus& bus, ecs::EventBus::EventToken token);

    /** @brief Moves the pending systems into their stage lists. */
    void mergePending();

    /** @brief Owning manager. */
    SceneManager* _manager;
    /** @brief Id of the scene. */
    SceneId _id;
    /** @brief Subscriptions to cancel on exit. */
    std::vector<Subscription> _subscriptions;
    /** @brief Systems, per stage. */
    std::array<std::vector<SceneSystem>, SCENE_STAGE_COUNT> _systems;
    /** @brief Systems added while systems were running. */
    std::vector<PendingSystem> _pending;
    /** @brief Depth of `runSystems` calls, to defer additions made while systems run. */
    std::size_t _runDepth = 0;
};

}  // namespace scene

#include "scene/SceneContext.tpp"  // IWYU pragma: export
