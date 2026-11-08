/**
 * @file ScriptSystem.hpp
 * @brief ECS system managing Wren script lifecycles, event forwarding, coroutines, and hot-reload.
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
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
#include "prefabs/PrefabManager.hpp"
#include "scripting/WrenVM.hpp"            // IWYU pragma: export
#include "scripting/detail/WrenTypes.hpp"  // IWYU pragma: export

namespace systems {

/**
 * @brief Represents a suspended Wren Fiber waiting for a timed delay (Engine.wait).
 */
/**
 * @brief Opaque identifier of what is running Wren code: a script entity, or anything an extension defines.
 *
 * Entity scripts use their entity handle (52 bits). Extensions use values with the top bit set (see
 * `ScriptSystem::EXTENSION_OWNER_BIT`) so they never collide with an entity.
 */
using ScriptOwner = std::uint64_t;

/** @brief `ScriptOwner` meaning "nobody": code run outside any script or extension call. */
inline constexpr ScriptOwner NO_SCRIPT_OWNER = std::numeric_limits<ScriptOwner>::max();

/** @brief Moment of a hot-reload VM swap reported to `ScriptSystem::addVmResetListener` listeners. */
enum class VmResetPhase : std::uint8_t {
    /** @brief The VM is about to be destroyed: release every handle into it. */
    Before,
    /** @brief The new VM is ready, with the core bindings, the replays and the reloaded modules. */
    After,
};

/**
 * @brief Represents a suspended Wren Fiber waiting for a timed delay (Engine.wait).
 */
struct SleepingFiber {
    /** @brief Owning RAII handle to the suspended Wren Fiber. */
    scripting::detail::UniqueWrenHandle fiber;
    /** @brief Remaining duration in seconds before resumption. */
    float timeRemaining = 0.0F;
    /** @brief Owner running when the fiber went to sleep; restored while it runs again. */
    ScriptOwner owner = NO_SCRIPT_OWNER;
};

/**
 * @brief System managing lifecycle, event propagation, and execution of Wren entity scripts.
 *
 * Responsibilities:
 * - Instantiates `ScriptComponent` instances when entities are spawned (`onSpawn()`).
 * - Dispatches engine events (`events::CollisionBeginEvent`, etc.) directly to magic script methods
 * (`onCollision(otherId)`).
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
     * @brief Forwards `events::CollisionBeginEvent` to the `onCollision(other)` method of both entities' scripts.
     *
     * Only scripts whose `ScriptComponent::hasCollision` is set are called. A contact is skipped for a side when
     * either entity is dead (killed by an earlier subscriber) or already queued for destruction this frame
     * (`Entity.destroy()`), so a script never hits an entity that another system or script just destroyed. Script
     * mutations stay deferred through the command buffer. Calling it again moves the subscription to `bus`.
     *
     * @param bus The EventBus receiving the physics contact events.
     */
    void forwardCollisions(ecs::EventBus& bus);

    /**
     * @brief Unsubscribes the collision forwarding set up by `forwardCollisions`, if any. Safe to call twice.
     * @throws std::bad_alloc `EventBus::unsubscribe` rebuilds the listener list.
     */
    void stopForwardingCollisions();

    /**
     * @brief Returns a reference to the embedded WrenVM wrapper.
     */
    [[nodiscard]] scripting::WrenVM& getVm() noexcept;

    /**
     * @brief Returns a reference to the deferred command buffer.
     */
    [[nodiscard]] ecs::CommandBuffer& getCommandBuffer() noexcept;

    /**
     * @brief Sets the prefab registry used by `Engine.spawnPrefab(name, x, y)`.
     *
     * Without a manager, `spawnPrefab` logs an error and spawns nothing.
     *
     * @param manager Non-owning pointer; must outlive this system (nullptr to detach).
     */
    void setPrefabManager(prefabs::PrefabManager const* manager) noexcept;

    /**
     * @brief Returns the prefab registry used by `Engine.spawnPrefab`, or nullptr if none is set.
     */
    [[nodiscard]] prefabs::PrefabManager const* getPrefabManager() const noexcept;

    /**
     * @brief Returns the scratch buffer reused by the `World` query bindings to avoid per-call allocations.
     */
    [[nodiscard]] std::vector<ecs::Entity>& getQueryBuffer() noexcept;

    /** @brief Whether an entity is queued for destruction by a script this frame. */
    [[nodiscard]] bool isPendingDestroy(ecs::Entity e) const noexcept;

    /** @brief Entities queued for destruction by a script this frame. */
    [[nodiscard]] std::span<ecs::Entity const> getPendingDestroyEntities() const noexcept {
        return _pendingDestroyEntities;
    }

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

    /** @brief Top bit of a `ScriptOwner` defined by an extension, so it never equals an entity handle. */
    static constexpr ScriptOwner EXTENSION_OWNER_BIT = ScriptOwner{1} << 63U;

    /**
     * @brief Called for every entity that Wren code spawns through `Engine.spawnPrefab`, once it is built.
     * @param context Pointer given to `setSpawnDecorator`.
     * @param registry Registry holding the entity.
     * @param spawned The new entity.
     * @param owner Owner that was running when the spawn was requested.
     */
    using SpawnDecorator = void (*)(void* context, ecs::Registry& registry, ecs::Entity spawned, ScriptOwner owner);

    /**
     * @brief Gets the owner of the Wren code running now.
     * @return The entity whose script runs, the owner set by an extension, or `NO_SCRIPT_OWNER`.
     */
    [[nodiscard]] ScriptOwner currentOwner() const noexcept { return _extensions.currentOwner; }

    /**
     * @brief Sets the owner of the Wren code about to run; fibers put to sleep and spawns record it.
     * @param owner New owner.
     * @return The previous owner, to restore once the call is done.
     */
    ScriptOwner setCurrentOwner(ScriptOwner owner) noexcept {
        const ScriptOwner previous = _extensions.currentOwner;
        _extensions.currentOwner = owner;
        return previous;
    }

    /**
     * @brief Drops the sleeping fibers of an owner, so they never run again.
     * @param owner Owner whose fibers to cancel.
     */
    void cancelFibers(ScriptOwner owner);

    /**
     * @brief Sets the callback run on every entity spawned from Wren, for instance to tag it with its owner.
     * @param decorator Callback, or null to remove it.
     * @param context Pointer passed back to the callback.
     */
    void setSpawnDecorator(SpawnDecorator decorator, void* context) noexcept {
        _extensions.spawnDecorator = decorator;
        _extensions.spawnContext = context;
    }

    /**
     * @brief Gets the callback run on entities spawned from Wren.
     * @return The callback, or null.
     */
    [[nodiscard]] SpawnDecorator spawnDecorator() const noexcept { return _extensions.spawnDecorator; }

    /**
     * @brief Gets the context passed to the spawn decorator.
     * @return The context given to `setSpawnDecorator`.
     */
    [[nodiscard]] void* spawnDecoratorContext() const noexcept { return _extensions.spawnContext; }

    /**
     * @brief Adds a built-in module that scripts can import by name, like `core/Engine`.
     *
     * Checked after the core modules and before the files on disk. Kept across hot reloads.
     * @param name Import name (e.g. `core/Scene`).
     * @param source Wren source of the module.
     */
    void addModuleSource(std::string name, std::string source);

    /**
     * @brief Imports a script module (built-in or `<name>.wren` on disk), and watches its file for hot reload.
     * @param moduleName Module name.
     * @return True if the module is loaded.
     */
    bool loadModule(const std::string& moduleName) { return ensureScriptModuleLoaded(moduleName); }

    /**
     * @brief Sets the directory module files are read from: module `a/B` is `<root>/a/B.wren`.
     *
     * Empty (the default) reads them from the working directory. Set it before the first module is loaded.
     * @param root Script root, usually `engine::contentRoot()`.
     */
    void setScriptRoot(std::filesystem::path root) { *_scriptRoot = std::move(root); }

    /**
     * @brief Gets the directory module files are read from.
     * @return Script root; empty for the working directory.
     */
    [[nodiscard]] const std::filesystem::path& scriptRoot() const noexcept { return *_scriptRoot; }

    /**
     * @brief Registers a callback told before and after a hot reload replaces the VM.
     * @param listener Callback receiving the phase.
     * @return Id for `removeVmResetListener`.
     */
    std::size_t addVmResetListener(std::function<void(VmResetPhase)> listener);

    /**
     * @brief Tells whether every watched module ran again without error in the last hot reload.
     *
     * Read by `VmResetPhase::After` listeners: when false, some module stopped before its end, so what it would
     * have redone (declarations, registrations) may be missing.
     * @return True when the last replay succeeded, or before any hot reload.
     */
    [[nodiscard]] bool lastReplaySucceeded() const noexcept { return _lastReplaySucceeded; }

    /**
     * @brief Removes a VM reset listener.
     * @param id Id returned by `addVmResetListener`; unknown ids are ignored.
     */
    void removeVmResetListener(std::size_t id);

  private:
    /** @brief Restores the previous script owner when it goes out of scope. */
    class OwnerScope {
      public:
        /**
         * @brief Sets `owner` as the current owner of `system`.
         * @param system System whose owner changes.
         * @param owner Owner for the scope.
         */
        OwnerScope(ScriptSystem& system, ScriptOwner owner) noexcept
            : _system(system), _previous(system.setCurrentOwner(owner)) {}
        /** @brief Restores the previous owner. */
        ~OwnerScope() { (void)_system.setCurrentOwner(_previous); }
        /** @brief Deleted copy constructor. */
        OwnerScope(const OwnerScope&) = delete;
        /** @brief Deleted copy assignment. */
        OwnerScope& operator=(const OwnerScope&) = delete;
        /** @brief Deleted move constructor. */
        OwnerScope(OwnerScope&&) = delete;
        /** @brief Deleted move assignment. */
        OwnerScope& operator=(OwnerScope&&) = delete;

      private:
        /** @brief System whose owner was changed. */
        ScriptSystem& _system;
        /** @brief Owner to restore. */
        ScriptOwner _previous;
    };

    /** @brief State used by engine extensions (owners, spawn decorator, reset listeners), moved as one block. */
    struct Extensions {
        /** @brief Owner of the Wren code running now. */
        ScriptOwner currentOwner = NO_SCRIPT_OWNER;
        /** @brief Callback run on entities spawned from Wren. */
        SpawnDecorator spawnDecorator = nullptr;
        /** @brief Context of `spawnDecorator`. */
        void* spawnContext = nullptr;
        /** @brief Hot-reload listeners, with their ids. */
        std::vector<std::pair<std::size_t, std::function<void(VmResetPhase)>>> vmResetListeners;
        /** @brief Next listener id. */
        std::size_t nextListenerId = 0;
        /** @brief Fibers woken this frame; capacity kept between frames. */
        std::vector<SleepingFiber> readyFibers;
    };

    /**
     * @brief Tells the VM reset listeners about a phase of a hot reload.
     * @param phase Phase to report.
     */
    void notifyVmReset(VmResetPhase phase);

    /**
     * @brief Calls `signature` (a one-argument method) on the script instance of `target`, passing `arg` wrapped in an
     * `Entity` (or as the raw number when `core/Entity` is not loaded). Does nothing if the instance or handle is
     * missing.
     * @param target Entity whose script instance receives the call.
     * @param signature Cached method signature (e.g. "onCollision(_)").
     * @param arg Numeric argument, kept as a `double` so values that are not entity ids are not truncated.
     */
    void invokeEntityMethod(ecs::Entity target, std::string const& signature, double arg);

    /** @brief Calls `onCollision(other)` on `target` if it opted in and both entities are alive and not pending
     * destruction. */
    void forwardCollision(ecs::Entity target, ecs::Entity other);

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
     * @brief Registers the registry destroy listener that releases script instances of killed entities.
     *
     * Entities destroyed by C++ code (game rules, physics, network despawn, ...) never go through
     * `queueDestroyEntity`. The listener runs their `onDestroy()` hook while their components still exist, then
     * releases the instance so it is neither updated nor kept alive once the entity is dead.
     * The listener captures `this`, so it is re-registered by the move operations.
     *
     * @warning The hook calls into Wren synchronously: `Registry::killEntity` must not be called from inside a
     *          foreign method. Scripts destroy entities through `queueDestroyEntity`, which is deferred.
     */
    void attachDestroyListener();

    /** @brief Unregisters the destroy listener registered by `attachDestroyListener`, if any. */
    void detachDestroyListener() noexcept;

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
    /** @brief Id of the registry destroy listener releasing script instances, if registered. */
    std::optional<std::size_t> _destroyListenerId;
    /** @brief Non-owning pointer to the EventBus. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Non-owning pointer to the prefab registry used by `Engine.spawnPrefab`. */
    prefabs::PrefabManager const* _prefabs = nullptr;
    /** @brief Internal command buffer buffering mutations during script execution. */
    ecs::CommandBuffer _commandBuffer;
    /** @brief Embedded Wren Virtual Machine instance. */
    scripting::WrenVM _vm;

    /** @brief Cache of entities opting in to per-frame update execution. */
    std::vector<ecs::Entity> _updatingEntities;
    /** @brief Queue of entities pending deferred destruction with onDestroy hook. */
    std::vector<ecs::Entity> _pendingDestroyEntities;
    /** @brief Scratch buffer reused by the `World` spatial query bindings. */
    std::vector<ecs::Entity> _queryBuffer;
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

    /** @brief Bus the collision forwarding is subscribed to, or nullptr. */
    ecs::EventBus* _collisionBus = nullptr;
    /** @brief Subscription token of the collision forwarding. */
    ecs::EventBus::EventToken _collisionToken = 0;
    /**
     * @brief Heap slot holding the system that receives the forwarded collisions.
     *
     * The bus callback reads the system through this slot instead of capturing `this`, so a move only redirects the
     * slot to the new instance. It never touches the bus, which allocates, and the moves stay `noexcept`.
     */
    std::unique_ptr<ScriptSystem*> _collisionTarget;

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
    /** @brief Whether every module of the last hot reload ran again without error. */
    bool _lastReplaySucceeded = true;
    /** @brief State used by engine extensions. */
    Extensions _extensions;
    /**
     * @brief Built-in modules added by extensions, by import name.
     * Shared with the module loader, which outlives moves of the system and is copied into the staging VMs.
     */
    std::shared_ptr<std::unordered_map<std::string, std::string>> _extraModules =
        std::make_shared<std::unordered_map<std::string, std::string>>();
    /** @brief Directory module files are read from; shared with the module loader, like `_extraModules`. */
    std::shared_ptr<std::filesystem::path> _scriptRoot = std::make_shared<std::filesystem::path>();
};

}  // namespace systems

#include "systems/script/ScriptSystem.tpp"  // IWYU pragma: export
