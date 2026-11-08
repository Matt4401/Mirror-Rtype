/**
 * @file AutoLobbyPlugin.hpp
 * @brief Plays the lobby without a menu: connects at start-up, joins the first open room, `ready` toggles readiness.
 */

#pragma once

#include <optional>
#include <string_view>

#include "ClientOptions.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace client {

/**
 * @brief Drives `RTypeNetworkPlugin` the way the client behaved before its menus.
 *
 * - `init` requests a connection to the server of the command line.
 * - In the lobby, joins the first room waiting for players, or creates one if there is none (and tries again from the
 *   next list if the join is refused).
 * - In a room, the `ready` action toggles the ready state.
 * - When the session closes, stops the engine; `failed()` then tells the exit code.
 */
class AutoLobbyPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param options Server to connect to.
     */
    explicit AutoLobbyPlugin(ClientOptions options) : _options(std::move(options)) {}

    /**
     * @brief Gets plugin name.
     * @return String view "AutoLobbyPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AutoLobbyPlugin"; }

    /**
     * @brief Declares the dependency on the session.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the session events and requests the connection.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Toggles the ready state when the `ready` action is pressed in a room.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Tells whether the engine stopped because the session closed.
     * @return `true` after a refused, failed or lost connection.
     */
    [[nodiscard]] bool failed() const noexcept { return _failed; }

  private:
    /** @brief Server to connect to. */
    ClientOptions _options;
    /** @brief Ready state last requested. */
    bool _ready = false;
    /** @brief The session closed. */
    bool _failed = false;
};

}  // namespace client
