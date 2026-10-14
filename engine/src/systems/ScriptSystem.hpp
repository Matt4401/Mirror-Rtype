#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "components/ScriptComponent.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace systems {

/**
 * @brief Represents a suspended Wren Fiber waiting for a timed delay (Engine.wait).
 */
struct SleepingFiber {
    /** @brief Owning RAII handle to the suspended Wren Fiber. */
    scripting::detail::UniqueWrenHandle fiber;
    /** @brief Remaining duration in seconds before resumption. */
    float timeRemaining = 0.0F;
};

/**
 * @brief System managing lifecycle, event propagation, and execution of Wren entity scripts.
 *
 * Responsibilities:
 * - Instantiates `ScriptComponent` instances when entities are spawned (`onSpawn()`).
 * - Dispatches engine events (`CollisionEvent`, etc.) directly to magic script methods (`onCollision(otherId)`).
 * - Manages asynchronous coroutines via `Engine.wait(seconds)`.
 * - Executes optional `update(dt)` loop for entities opting in via `hasUpdate = true`.
 * - Flushes deferred ECS mutations safely using an internal `CommandBuffer`.
 */
class ScriptSystem {
  public:
    ScriptSystem();
    ~ScriptSystem();

    ScriptSystem(const ScriptSystem&) = delete;
    ScriptSystem& operator=(const ScriptSystem&) = delete;

    ScriptSystem(ScriptSystem&& other) noexcept;
    ScriptSystem& operator=(ScriptSystem&& other) noexcept;

    /**
     * @brief Initializes the ScriptSystem, registers core bindings, and loads built-in modules.
     *
     * @param registry Reference to the active ECS Registry.
     * @param bus Reference to the active EventBus.
     */
    void init(ecs::Registry& registry, ecs::EventBus& bus);

    /**
     * @brief Executes frame updates: spawns new scripts, advances fibers, runs opt-in update loops, and flushes
     * mutations.
     *
     * @param dt Delta time in seconds since previous frame.
     */
    void update(float dt);

    /**
     * @brief Instantiates a script instance for an entity and invokes `onSpawn()` if implemented.
     *
     * @param e Entity owning the script component.
     * @param script Reference to the entity's ScriptComponent.
     */
    void instantiateScript(ecs::Entity e, ecs::ScriptComponent& script);

    /**
     * @brief Binds a C++ ECS EventBus event type to an entity script method.
     *
     * Automatically extracts the target entity and argument entity/value using `extractor`,
     * then calls `methodName(_)` on the target entity's script instance.
     *
     * @tparam EventType Event struct/class subscribed on the EventBus.
     * @tparam ArgExtractor Callable returning a std::pair<ecs::Entity, ArgType>.
     * @param bus The EventBus to subscribe to.
     * @param methodName The name of the method to invoke on the Wren script (e.g. "onCollision").
     * @param extractor Extractor callable extracting target and argument entities from the event.
     */
    template <typename EventType, typename ArgExtractor>
    void registerEventForwarding(ecs::EventBus& bus, std::string const& methodName, ArgExtractor&& extractor);

    /**
     * @brief Returns a reference to the embedded WrenVM wrapper.
     */
    [[nodiscard]] scripting::WrenVM& getVm() noexcept;

    /**
     * @brief Returns a reference to the deferred command buffer.
     */
    [[nodiscard]] ecs::CommandBuffer& getCommandBuffer() noexcept;

    /**
     * @brief Registers a suspended Wren Fiber to resume after `seconds` have elapsed.
     *
     * @param fiber Raw pointer to the suspended Wren Fiber handle.
     * @param seconds Sleep duration in seconds.
     */
    void addSleepingFiber(WrenHandle* fiber, float seconds);

  private:
    void registerCoreBindings();
    void loadCoreModules();

    ecs::Registry* _registry = nullptr;
    ecs::EventBus* _bus = nullptr;
    ecs::CommandBuffer _commandBuffer;
    scripting::WrenVM _vm;

    std::vector<ecs::Entity> _updatingEntities;
    std::vector<SleepingFiber> _sleepingFibers;

    scripting::detail::UniqueWrenHandle _newHandle;
    scripting::detail::UniqueWrenHandle _updateHandle;
    scripting::detail::UniqueWrenHandle _onSpawnHandle;
    scripting::detail::UniqueWrenHandle _fiberCallHandle;

    std::unordered_map<std::string, scripting::detail::UniqueWrenHandle> _cachedMethodHandles;
};

}  // namespace systems

#include "ScriptSystem.tpp"
