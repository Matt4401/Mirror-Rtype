/**
 * @file Engine.hpp
 * @brief Central coordinator managing ECS, events, plugins, and the main game loop.
 */

#pragma once

#include <expected>
#include <functional>

#include "core/IPlugin.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginGraph.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"

namespace engine {

/**
 * @brief Core engine coordinator integrating Registry, EventBus, Mediator, and Plugins.
 */
class Engine {
  public:
    /**
     * @brief Constructs Engine with optional custom time source for Clock.
     * @param timeSource Monotonic time query function (defaults to real steady time).
     */
    explicit Engine(ecs::time::Clock::TimeSource timeSource = nullptr);

    /** @brief Destroys Engine and shuts down plugins if initialized. */
    ~Engine();

    /** @brief Deleted copy constructor. */
    Engine(const Engine&) = delete;

    /** @brief Deleted copy assignment operator. */
    Engine& operator=(const Engine&) = delete;

    /** @brief Deleted move constructor. */
    Engine(Engine&&) noexcept = delete;

    /** @brief Deleted move assignment operator. */
    Engine& operator=(Engine&&) noexcept = delete;

    /**
     * @brief Adds and instantiates a plugin into the engine.
     * @tparam Plugin Concrete plugin class deriving from IPlugin.
     * @tparam Args Constructor argument types for Plugin.
     * @param args Arguments forwarded to Plugin's constructor.
     * @return Reference to this Engine for method chaining.
     */
    template <typename Plugin, typename... Args>
    Engine& addPlugin(Args&&... args);

    /**
     * @brief Checks if a plugin of the given type has been added.
     * @tparam Plugin Plugin class type.
     * @return True if present.
     */
    template <typename Plugin>
    [[nodiscard]] bool hasPlugin() const noexcept;

    /**
     * @brief Retrieves a reference to an added plugin.
     * @tparam Plugin Plugin class type.
     * @return Reference to the plugin instance.
     */
    template <typename Plugin>
    [[nodiscard]] Plugin& getPlugin();

    /**
     * @brief Resolves an abstract service from the internal Mediator.
     * @tparam Service Interface type to query.
     * @return Reference to the service.
     * @throws std::runtime_error If the service is not found.
     */
    template <typename Service>
    [[nodiscard]] Service& service() const {
        return _mediator.service<Service>();
    }

    /**
     * @brief Attempts to resolve an abstract service from the internal Mediator without throwing.
     * @tparam Service Interface type to query.
     * @return Reference wrapper to service, or MediatorError if absent.
     */
    template <typename Service>
    [[nodiscard]] std::expected<std::reference_wrapper<Service>, MediatorError> tryService() const noexcept {
        return _mediator.getService<Service>();
    }

    /** @brief Gets mutable reference to the central ECS Registry. */
    [[nodiscard]] ecs::Registry& registry() noexcept { return _registry; }

    /** @brief Gets const reference to the central ECS Registry. */
    [[nodiscard]] const ecs::Registry& registry() const noexcept { return _registry; }

    /** @brief Gets mutable reference to the central EventBus. */
    [[nodiscard]] ecs::EventBus& eventBus() noexcept { return _eventBus; }

    /** @brief Gets mutable reference to the Mediator service registry. */
    [[nodiscard]] Mediator& mediator() noexcept { return _mediator; }

    /** @brief Gets const reference to the Mediator service registry. */
    [[nodiscard]] const Mediator& mediator() const noexcept { return _mediator; }

    /** @brief Gets mutable reference to the simulation clock. */
    [[nodiscard]] ecs::time::Clock& clock() noexcept { return _clock; }

    /**
     * @brief Sets the duration of fixed physics/simulation steps.
     * @param fixedDt Step duration in seconds.
     */
    void setFixedDeltaTime(float fixedDt) noexcept { _fixedDt = fixedDt; }

    /**
     * @brief Gets the fixed simulation step duration in seconds.
     * @return Step duration.
     */
    [[nodiscard]] float fixedDeltaTime() const noexcept { return _fixedDt; }

    /**
     * @brief Resolves dependencies and initializes all plugins in topological order.
     */
    void init();

    /**
     * @brief Executes a single frame step (useful for testing and headless servers).
     * @param dt Frame delta time in seconds.
     */
    void step(float dt);

    /**
     * @brief Runs the continuous main game loop until stopped or window is closed.
     */
    void run();

    /**
     * @brief Signals the main game loop to terminate.
     */
    void stop() noexcept { _running = false; }

    /**
     * @brief Checks if the engine loop is currently running.
     * @return True if running.
     */
    [[nodiscard]] bool isRunning() const noexcept { return _running; }

    /**
     * @brief Gets the ordered list of registered plugins in topological execution sequence.
     * @return Const reference to execution order vector.
     */
    [[nodiscard]] const std::vector<IPlugin*>& executionOrder() const { return _pluginGraph.executionOrder(); }

    /**
     * @brief Shuts down all initialized plugins in reverse topological order.
     */
    void shutdown();

  private:
    /**
     * @brief Automatically adds mandatory default plugins if they were not registered yet.
     */
    void ensureDefaultPlugins();

    /** @brief Central ECS registry holding all components and resources. */
    ecs::Registry _registry;
    /** @brief Event dispatch bus coordinating intra-engine and game events. */
    ecs::EventBus _eventBus;
    /** @brief Mediator service locator decoupling plugin service interactions. */
    Mediator _mediator;
    /** @brief Dependency resolution graph determining plugin execution sequence. */
    PluginGraph _pluginGraph;
    /** @brief High-precision monotonic simulation clock. */
    ecs::time::Clock _clock;
    /** @brief Fixed timestep duration in seconds for physics and fixed update. */
    float _fixedDt = ecs::time::DEFAULT_FIXED_DT;
    /** @brief Flag indicating whether plugins have completed the init phase. */
    bool _initialized = false;
    /** @brief Flag controlling the active game loop execution state. */
    bool _running = false;
};

}  // namespace engine

#include "core/Engine.tpp"  // IWYU pragma: export
