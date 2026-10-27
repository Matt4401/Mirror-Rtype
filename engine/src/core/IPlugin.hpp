/**
 * @file IPlugin.hpp
 * @brief Base interface for modular engine plugins with lifecycle stages.
 */

#pragma once

#include <string_view>

#include "core/PluginBuilder.hpp"

namespace ecs::time {
struct Time;
}

namespace engine {

class Engine;

/**
 * @brief Modular extension interface providing lifecycle stages and dependency management.
 *
 * Plugins declare dependencies during `build()`, register services and initialize resources
 * during `init()`, and participate in fixed and frame updates before releasing state in `shutdown()`.
 */
class IPlugin {
  public:
    /** @brief Default constructor. */
    IPlugin() = default;

    /** @brief Virtual destructor. */
    virtual ~IPlugin() = default;

    /** @brief Deleted copy constructor. */
    IPlugin(const IPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    IPlugin& operator=(const IPlugin&) = delete;

    /** @brief Deleted move constructor. */
    IPlugin(IPlugin&&) noexcept = delete;

    /** @brief Deleted move assignment operator. */
    IPlugin& operator=(IPlugin&&) noexcept = delete;

    /**
     * @brief Unique human-readable name of the plugin for debugging and diagnostics.
     * @return String view of the plugin name.
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * @brief Declares dependencies on other plugins.
     * @param builder Fluent configuration builder to declare requires/optionally constraints.
     */
    virtual void build(PluginBuilder& builder) { (void)builder; }

    /**
     * @brief Initializes resources and registers services in the Engine and Mediator.
     * @param engine Reference to the hosting Engine.
     */
    virtual void init(Engine& engine) { (void)engine; }

    /**
     * @brief Fixed-timestep simulation tick (called per constant fixedDt interval).
     * @param engine Reference to the hosting Engine.
     * @param time Fixed simulation timing information.
     */
    virtual void fixedUpdate(Engine& engine, const ecs::time::Time& time) {
        (void)engine;
        (void)time;
    }

    /**
     * @brief Variable-timestep frame logic tick.
     * @param engine Reference to the hosting Engine.
     * @param time Frame timing information.
     */
    virtual void update(Engine& engine, const ecs::time::Time& time) {
        (void)engine;
        (void)time;
    }

    /**
     * @brief Frame rendering phase.
     * @param engine Reference to the hosting Engine.
     */
    virtual void render(Engine& engine) { (void)engine; }

    /**
     * @brief Graceful shutdown and resource release (called in reverse topological order).
     * @param engine Reference to the hosting Engine.
     */
    virtual void shutdown(Engine& engine) { (void)engine; }
};

}  // namespace engine
