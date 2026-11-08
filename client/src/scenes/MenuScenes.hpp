/**
 * @file MenuScenes.hpp
 * @brief Scenes of the R-Type client before the lobby: title, server address, settings.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ecs/Entity.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneTypes.hpp"

namespace client {

/** @brief Title screen: Play, Settings, Quit. */
class TitleScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Title".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Title"; }

    /**
     * @brief Spawns the title and the buttons.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

    /**
     * @brief Focuses the first button again.
     * @param context Scene context.
     */
    void onResume(scene::SceneContext& context) override;

  private:
    /** @brief "Play" button, focused first. */
    ecs::Entity _play = ecs::NULL_ENTITY;
};

/**
 * @brief Server address: host and port fields, then connection progress or the reason it failed.
 *
 * The fields start from the last server the player reached (saved in the settings), else from the command line. A
 * successful connection saves them and goes to the lobby.
 */
class ConnectScene final : public scene::IScene {
  public:
    /**
     * @brief Creates the scene.
     * @param host Host proposed when nothing is saved.
     * @param port Port proposed when nothing is saved.
     * @param preferDefaults `true` to propose `host` and `port` even if a server is saved (given on the command line).
     */
    ConnectScene(std::string host, std::uint16_t port, bool preferDefaults);

    /**
     * @brief Gets the scene name.
     * @return "Connect".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Connect"; }

    /**
     * @brief Spawns the form, shows the last error if any.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /**
     * @brief Reads the fields and requests the connection.
     * @param context Scene context.
     */
    void connect(scene::SceneContext& context) const;

    /** @brief Host proposed when nothing is saved. */
    std::string _host;
    /** @brief Port proposed when nothing is saved. */
    std::uint16_t _port;
    /** @brief Propose the defaults even if a server is saved. */
    bool _preferDefaults;
    /** @brief Host field. */
    ecs::Entity _hostField = ecs::NULL_ENTITY;
    /** @brief Port field. */
    ecs::Entity _portField = ecs::NULL_ENTITY;
    /** @brief Progress or error line. */
    ecs::Entity _status = ecs::NULL_ENTITY;
    /** @brief "Connect" button. */
    ecs::Entity _connectButton = ecs::NULL_ENTITY;
};

/** @brief Settings: volumes and the metrics panel, saved at once. Pushed from the title or the pause menu. */
class SettingsScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Settings".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Settings"; }

    /**
     * @brief Spawns the widgets from the current settings.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;
};

}  // namespace client
