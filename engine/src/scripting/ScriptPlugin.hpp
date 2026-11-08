/**
 * @file ScriptPlugin.hpp
 * @brief Scripting lifecycle plugin embedding Wren VM and entity script execution.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace scripting {

/** @brief When the scripts run. */
enum class ScriptTick : std::uint8_t {
    /** @brief Every frame, in `update()`, with the frame's delta time: for a client, in step with what is drawn. */
    EveryFrame,
    /**
     * @brief Every fixed step, in `fixedUpdate()`, with the fixed delta time: for a server, so scripts follow the
     * simulation rate, deterministically, however often the engine is advanced.
     */
    FixedStep,
};

/**
 * @brief Plugin responsible for Wren VM integration, script component lifecycle, and hot-reload.
 */
class ScriptPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Constructs ScriptPlugin.
     * @param tick When the scripts run; every frame by default.
     */
    explicit ScriptPlugin(ScriptTick tick = ScriptTick::EveryFrame) noexcept : _tick(tick) {}

    /** @brief Destructor. */
    ~ScriptPlugin() override = default;

    /** @brief Deleted copy constructor. */
    ScriptPlugin(const ScriptPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    ScriptPlugin& operator=(const ScriptPlugin&) = delete;

    /** @brief Deleted move constructor. */
    ScriptPlugin(ScriptPlugin&&) = delete;

    /** @brief Deleted move assignment operator. */
    ScriptPlugin& operator=(ScriptPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "ScriptPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ScriptPlugin"; }

    /**
     * @brief Declares plugin dependencies.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Initializes Wren VM bindings and core modules against host Engine.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Executes script lifecycle updates, fiber sleeps, and deferred commands, with `ScriptTick::EveryFrame`.
     * @param engine Reference to host Engine.
     * @param time Frame timing information.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Same as `update()`, with `ScriptTick::FixedStep`.
     * @param engine Reference to host Engine.
     * @param time Fixed step timing information.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Gets when the scripts run.
     * @return The tick given at construction.
     */
    [[nodiscard]] ScriptTick tick() const noexcept { return _tick; }

    /**
     * @brief Shuts down scripting runtime: stops forwarding collision events to the scripts.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets mutable reference to the underlying ScriptSystem. */
    [[nodiscard]] systems::ScriptSystem& scriptSystem() noexcept { return _scriptSystem; }

    /** @brief Gets const reference to the underlying ScriptSystem. */
    [[nodiscard]] const systems::ScriptSystem& scriptSystem() const noexcept { return _scriptSystem; }

  private:
    /** @brief Internal ScriptSystem instance driving Wren VM execution. */
    systems::ScriptSystem _scriptSystem;
    /** @brief When the scripts run. */
    ScriptTick _tick = ScriptTick::EveryFrame;
};

}  // namespace scripting
