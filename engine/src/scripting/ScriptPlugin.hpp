/**
 * @file ScriptPlugin.hpp
 * @brief Scripting lifecycle plugin embedding Wren VM and entity script execution.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace scripting {

/**
 * @brief Plugin responsible for Wren VM integration, script component lifecycle, and hot-reload.
 */
class ScriptPlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs ScriptPlugin. */
    ScriptPlugin() = default;

    /** @brief Destructor. */
    ~ScriptPlugin() override = default;

    /** @brief Deleted copy constructor. */
    ScriptPlugin(const ScriptPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    ScriptPlugin& operator=(const ScriptPlugin&) = delete;

    /** @brief Move constructor. */
    ScriptPlugin(ScriptPlugin&&) noexcept = default;

    /** @brief Move assignment operator. */
    ScriptPlugin& operator=(ScriptPlugin&&) noexcept = default;

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
     * @brief Executes script lifecycle updates, fiber sleeps, and deferred commands.
     * @param engine Reference to host Engine.
     * @param time Frame timing information.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Shuts down scripting runtime.
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
};

}  // namespace scripting
