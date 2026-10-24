/**
 * @file ScriptSystem.hpp
 * @brief ECS system managing Wren script lifecycles, event forwarding, coroutines, and hot-reload.
 */

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
    /**
     * @brief Constructs a ScriptSystem.
     */
    ScriptSystem();

    /**
     * @brief Destroys the ScriptSystem and releases all cached script and method handles.
     */
    ~ScriptSystem();

    /** @brief Deleted copy constructor. */
    ScriptSystem(const ScriptSystem&) = delete;
    /** @brief Deleted copy assignment operator. */
    ScriptSystem& operator=(const ScriptSystem&) = delete;

    /**
     * @brief Move constructor transferring VM, handles, sleeping fibers, and watched files.
     * @param other Rvalue reference to source ScriptSystem.
     */
    ScriptSystem(ScriptSystem&& other) noexcept;

    /**
     * @brief Move assignment operator transferring VM, handles, sleeping fibers, and watched files.
     * @param other Rvalue reference to source ScriptSystem.
     * @return Reference to this system.
     */
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
     * @param isReload True if re-instantiating following a hot-reload cycle, false on initial spawn.
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
     * @brief Returns a non-owning pointer to the active ECS Registry.
     */
    [[nodiscard]] ecs::Registry* getRegistry() noexcept { return _registry; }

    /**
     * @brief Retrieves the Wren script instance handle for an entity, or nullptr if none.
     */
    [[nodiscard]] ::WrenHandle* getScriptInstance(ecs::Entity e) const noexcept;

    /**
     * @brief Checks if a Wren script instance exists for an entity.
     */
    [[nodiscard]] bool hasScriptInstance(ecs::Entity e) const noexcept;

    /**
     * @brief Registers a suspended Wren Fiber to resume after `seconds` have elapsed.
     *
     * @param fiber Owning RAII handle to the suspended Wren Fiber.
     * @param seconds Sleep duration in seconds.
     */
    void addSleepingFiber(scripting::detail::UniqueWrenHandle fiber, float seconds);

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
    /**
     * @brief Registers built-in engine foreign functions and classes with the VM.
     * @param vm Target WrenVM instance.
     */
    static void registerCoreBindings(scripting::WrenVM& vm);

    /**
     * @brief Loads and executes the embedded core modules (core/Engine, core/Entity, core/World, _internal).
     */
    void loadCoreModules();

    /**
     * @brief Compiles module source in a staging VM to verify validity before applying hot-reload.
     * @param moduleName Module name.
     * @param source Wren source code to validate.
     * @return True if valid, false if compilation failed.
     */
    bool validateScriptSource(std::string_view moduleName, std::string_view source);

    /**
     * @brief Decrements remaining sleep times and resumes expired coroutine fibers.
     * @param dt Elapsed frame delta time in seconds.
     */
    void updateSleepingFibers(float dt);

    /**
     * @brief Advances entities with active scripts opting in to update loops.
     * @param dt Elapsed frame delta time in seconds.
     */
    void updateScriptEntities(float dt);

    /**
     * @brief Processes queued entity destructions and flushes deferred mutations.
     */
    void flushPendingDestructions();

    /**
     * @brief Ensures a script module is compiled and imported into the VM, tracking it for changes.
     * @param moduleName Script module path/name.
     * @return True on success, false if file missing or compile failed.
     */
    bool ensureScriptModuleLoaded(const std::string& moduleName);

    /**
     * @brief Instantiates a Wren class object for an entity and returns the owning handle.
     * @param e Target entity.
     * @param script Reference to entity's ScriptComponent.
     * @return Owning UniqueWrenHandle to the new instance, or null handle on failure.
     */
    [[nodiscard]] scripting::detail::UniqueWrenHandle createScriptInstance(ecs::Entity e,
                                                                           const ecs::ScriptComponent& script);

    /**
     * @brief Scans all watched script files for timestamp changes and reads updated content.
     * @return List of module names and modified source strings.
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> collectModifiedModules();

    /**
     * @brief Releases active handles before tearing down the VM during hot-reload.
     */
    void releaseActiveHandles();

    /**
     * @brief Reinitializes the Wren VM, restores core bindings, and loads updated module code.
     * @param modifiedModules Modified module source list to compile into the new VM.
     */
    void recreateVmAndModules(const std::vector<std::pair<std::string, std::string>>& modifiedModules);

    /**
     * @brief Re-instantiates script objects for all active entities across the ECS registry.
     */
    void reinstantiateActiveScripts();

    /** @brief Non-owning pointer to the ECS Registry. */
    ecs::Registry* _registry = nullptr;
    /** @brief Non-owning pointer to the EventBus. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Internal command buffer buffering mutations during script execution. */
    ecs::CommandBuffer _commandBuffer;
    /** @brief Embedded Wren Virtual Machine instance. */
    scripting::WrenVM _vm;

    /** @brief Cache of entities opting in to per-frame update execution. */
    std::vector<ecs::Entity> _updatingEntities;
    /** @brief Queue of entities pending deferred destruction with onDestroy hook. */
    std::vector<ecs::Entity> _pendingDestroyEntities;
    /** @brief Collection of suspended coroutine fibers awaiting timeout. */
    std::vector<SleepingFiber> _sleepingFibers;
    /** @brief Active script instance handles indexed by entity ID. */
    std::unordered_map<ecs::Entity, scripting::detail::UniqueWrenHandle> _instanceHandles;

    /** @brief Call handle for class instantiation constructor (`new(_)`). */
    scripting::detail::UniqueWrenHandle _newHandle;
    /** @brief Call handle for frame update (`update(_)`). */
    scripting::detail::UniqueWrenHandle _updateHandle;
    /** @brief Call handle for spawn lifecycle hook (`tryCallSpawn(_)`). */
    scripting::detail::UniqueWrenHandle _onSpawnHandle;
    /** @brief Call handle for resuming suspended fibers (`call()`). */
    scripting::detail::UniqueWrenHandle _fiberCallHandle;

    /** @brief Cache of pre-compiled method call handles indexed by signature. */
    std::unordered_map<std::string, scripting::detail::UniqueWrenHandle> _cachedMethodHandles;

    /**
     * @brief Internal tracking structure monitoring script file modification timestamps on disk.
     */
    struct ScriptFileWatch {
        /** @brief File path of the monitored script. */
        std::string filePath;
        /** @brief Last recorded filesystem modification timestamp. */
        std::filesystem::file_time_type lastWriteTime;
        /** @brief Last successfully compiled and executed source code string. */
        std::string lastGoodSource;
    };

    /** @brief Map of module names to their tracked script file metadata. */
    std::unordered_map<std::string, ScriptFileWatch> _watchedScripts;
    /** @brief Entities that had active script instances recorded prior to reload. */
    std::vector<ecs::Entity> _entitiesToReinstantiate;
    /** @brief List of binder replays to re-execute when recreating the VM. */
    std::vector<std::function<void(scripting::WrenVM&)>> _binderReplayList;
    /** @brief Timer accumulating delta time between hot-reload disk polls. */
    float _reloadCheckTimer = 0.0F;
    /** @brief Configuration flag enabling or disabling hot-reloading. */
    bool _hotReloadEnabled = true;
};

}  // namespace systems

#include "systems/script/ScriptSystem.tpp"  // IWYU pragma: export
