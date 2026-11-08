/**
 * @file MenuScenes.hpp
 * @brief Scenes of the R-Type client before the lobby: title, server address, settings, controls.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "input/RTypeBindings.hpp"
#include "scene/IScene.hpp"

namespace client {

/** @brief Size of a table indexed by `engine::Key`. */
inline constexpr std::size_t KEY_SLOTS = 256;

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

/**
 * @brief Settings: volumes, metrics panel, accessibility options (auto-fire, text size) and the controls screen,
 * saved at once. Pushed from the title or the pause menu.
 */
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

  private:
    /** @brief Set when the scene is rebuilt after a text size change, to keep the focus on that button. */
    bool _focusScale = false;
};

/**
 * @brief Controls: one button per control showing its key; select one, press a key, and it is saved. Pushed from
 * the settings.
 */
class ControlsScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Controls".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Controls"; }

    /**
     * @brief Spawns a button per control and starts listening for keys.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /**
     * @brief Writes the current key of every control on its button.
     * @param context Scene context.
     */
    void refreshLabels(scene::SceneContext& context) const;

    /**
     * @brief Looks for a key pressed this frame while a control waits for one, and binds it.
     * @param context Scene context.
     */
    void pollKeys(scene::SceneContext& context);

    /** @brief Button of each control, indexed by `input::Control`. */
    std::array<ecs::Entity, input::CONTROL_COUNT> _buttons{};
    /** @brief Control waiting for a key, if any. */
    std::optional<input::Control> _waiting;
    /** @brief Keys down at the previous frame, indexed by `engine::Key`, so only new presses count. */
    std::array<bool, KEY_SLOTS> _wasDown{};
    /** @brief Key just bound: clicks are ignored until it is released, so Enter or Space do not start a new wait. */
    engine::Key _justBound = engine::Key::Unknown;
};

}  // namespace client
