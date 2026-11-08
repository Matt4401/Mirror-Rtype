/**
 * @file ClientScenesPlugin.hpp
 * @brief Registers the scenes of the R-Type client and applies its settings.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace client {

/**
 * @brief Assembles the R-Type menus on the engine scene system.
 *
 * - Registers the scenes (title, connect, settings, lobby, room, game, pause, game over) and the scripted
 *   `LevelTransition` overlay (`scripts/scenes/LevelTransition.wren`), then opens the title screen.
 * - Registers the `level_banner` prefab the overlay spawns, and gives the prefabs to the scripts.
 * - Declares the client settings; applies the volumes and the metrics panel on `SettingChanged`. F3 toggles the
 *   metrics setting.
 *
 * Scenes never call the session: they publish requests and react to its results (`SessionEvents.hpp`).
 */
class ClientScenesPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param host Server proposed on the connect screen.
     * @param port Port proposed on the connect screen.
     * @param fromCommandLine `true` if they come from the command line: they win over the last server saved.
     */
    ClientScenesPlugin(std::string host, std::uint16_t port, bool fromCommandLine);

    /**
     * @brief Gets plugin name.
     * @return String view "ClientScenesPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ClientScenesPlugin"; }

    /**
     * @brief Declares the plugins the scenes use.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Declares the settings, registers the prefab and the scenes, opens the title screen.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Toggles the metrics setting on F3.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

  private:
    /** @brief Server proposed on the connect screen. */
    std::string _host;
    /** @brief Port proposed on the connect screen. */
    std::uint16_t _port;
    /** @brief The proposed server comes from the command line. */
    bool _fromCommandLine;
    /** @brief F3 was held during the previous frame. */
    bool _metricsKeyHeld = false;
};

}  // namespace client
