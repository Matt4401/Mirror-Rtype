#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "components/ScriptComponent.hpp"
#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenTypes.hpp"

extern "C" {
#include <wren.h>
}

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
    void instantiateScript(ecs::Entity e, ecs::ScriptComponent& script, bool isReload = false);

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

    /**
     * @brief Enables or disables runtime hot-reloading of script files.
     * @param enabled True to poll for file changes, false to disable.
     */
    void enableHotReload(bool enabled) noexcept;

    /**
     * @brief Checks if hot-reloading is currently enabled.
     */
    [[nodiscard]] bool isHotReloadEnabled() const noexcept;

    /**
     * @brief Checks tracked script files on disk and reloads any modified scripts safely.
     * @return True if one or more scripts were reloaded, false otherwise.
     */
    bool reloadModifiedScripts();

    /**
     * @brief Invokes the optional `onDestroy()` script hook on an entity before removal.
     * @param e Entity being destroyed.
     */
    void triggerDestroyHook(ecs::Entity e);

    /**
     * @brief Queues an entity for destruction with deferred command execution and onDestroy hook.
     * @param e Entity to destroy.
     */
    void queueDestroyEntity(ecs::Entity e);

    /**
     * @brief Registers a binder replay callback invoked when the VM is cleanly swapped during hot-reload.
     * @param callback Function re-registering component bindings on the new WrenVM.
     */
    void addBinderReplay(std::function<void(scripting::WrenVM&)> callback);

  private:
    static void registerCoreBindings(scripting::WrenVM& vm);
    void loadCoreModules();
    bool validateScriptSource(std::string_view moduleName, std::string_view source);

    void updateSleepingFibers(float dt);
    void updateScriptEntities(float dt);
    void flushPendingDestructions();

    bool ensureScriptModuleLoaded(const std::string& moduleName);
    WrenHandle* createScriptInstanceHandle(ecs::Entity e, const ecs::ScriptComponent& script);

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> collectModifiedModules();
    void releaseActiveHandles();
    void recreateVmAndModules(const std::vector<std::pair<std::string, std::string>>& modifiedModules);
    void reinstantiateActiveScripts();

    ecs::Registry* _registry = nullptr;
    ecs::EventBus* _bus = nullptr;
    ecs::CommandBuffer _commandBuffer;
    scripting::WrenVM _vm;

    std::vector<ecs::Entity> _updatingEntities;
    std::vector<ecs::Entity> _pendingDestroyEntities;
    std::vector<SleepingFiber> _sleepingFibers;

    scripting::detail::UniqueWrenHandle _newHandle;
    scripting::detail::UniqueWrenHandle _updateHandle;
    scripting::detail::UniqueWrenHandle _onSpawnHandle;
    scripting::detail::UniqueWrenHandle _fiberCallHandle;

    std::unordered_map<std::string, scripting::detail::UniqueWrenHandle> _cachedMethodHandles;

    struct ScriptFileWatch {
        std::string filePath;
        std::filesystem::file_time_type lastWriteTime;
    };
    std::unordered_map<std::string, ScriptFileWatch> _watchedScripts;
    std::vector<std::function<void(scripting::WrenVM&)>> _binderReplayList;
    float _reloadCheckTimer = 0.0F;
    bool _hotReloadEnabled = true;
};

}  // namespace systems

#include "systems/script/ScriptSystem.tpp"  // IWYU pragma: export
