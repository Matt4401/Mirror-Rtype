/**
 * @file ConsolePlugin.hpp
 * @brief Plugin owning the CVar registry and the console interpreter. Headless: servers use it too.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "tools/console/ConsoleTypes.hpp"

namespace tools {

class CVarManager;
class Console;

/**
 * @brief Owns the `CVarManager` and the `Console` of the program, both Mediator services.
 *
 * - Other plugins register their cvars and commands in their `init` (declare `optionally<ConsolePlugin>()` to be
 *   initialized after it), or through `cvars()` / `console()` before `Engine::init`.
 * - The built-in commands (`help`, `set`, `cvar_list`...) are added at construction.
 * - `preUpdate` publishes one `CVarChanged` per changed variable.
 *
 * This plugin draws nothing: a UI plugin shows the console on the client, a terminal reader can feed it on a server.
 */
class ConsolePlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin, its registry and its console.
     * @param config Settings.
     */
    explicit ConsolePlugin(ConsoleConfig config = {});

    /** @brief Default destructor. */
    ~ConsolePlugin() override;
    /** @brief Not copyable. */
    ConsolePlugin(const ConsolePlugin&) = delete;
    /** @brief Not copyable. */
    ConsolePlugin& operator=(const ConsolePlugin&) = delete;
    /** @brief Not movable. */
    ConsolePlugin(ConsolePlugin&&) = delete;
    /** @brief Not movable. */
    ConsolePlugin& operator=(ConsolePlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "ConsolePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ConsolePlugin"; }

    /**
     * @brief Declares the core plugin and the two services.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers the registry and the console in the Mediator.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Publishes the cvar changes since the last frame.
     * @param engine Host engine.
     */
    void preUpdate(engine::Engine& engine) override;

    /**
     * @brief Removes the services.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets the registry. @return The CVar registry, usable before `init`. */
    [[nodiscard]] CVarManager& cvars() noexcept { return *_cvars; }

    /** @brief Gets the console. @return The console interpreter, usable before `init`. */
    [[nodiscard]] Console& console() noexcept { return *_console; }

  private:
    /** @brief CVar registry, shared with the Mediator. */
    std::shared_ptr<CVarManager> _cvars;
    /** @brief Console interpreter, shared with the Mediator. */
    std::shared_ptr<Console> _console;
};

}  // namespace tools
