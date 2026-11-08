/**
 * @file MenuScenes.hpp
 * @brief The RLSS scenes.
 */

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "Controls.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneTypes.hpp"

namespace rlss::client {

/** @brief Name entry: the title, a field and "VALIDER". Skipped when a name is saved. */
class NameScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Name".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Name"; }

    /**
     * @brief Spawns the screen, the field holding the current name.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /** @brief Name field. */
    ecs::Entity _field = ecs::NULL_ENTITY;
};

/** @brief Main menu: new match, browse, settings, name in the corner, players online. */
class MenuScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Menu".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Menu"; }

    /**
     * @brief Spawns the menu.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

    /**
     * @brief Focuses the first button again.
     * @param context Scene context.
     */
    void onResume(scene::SceneContext& context) override;

  private:
    /** @brief First button. */
    ecs::Entity _first = ecs::NULL_ENTITY;
};

/** @brief Matches in progress: one row per match (score, clock, players), scrollable; join or create. */
class BrowserScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Browser".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Browser"; }

    /**
     * @brief Spawns the list from the last matches received.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /**
     * @brief Replaces the rows with the current matches.
     * @param context Scene context.
     */
    void showMatches(scene::SceneContext& context);

    /**
     * @brief Focuses the row of a match after the rows were rebuilt.
     * @param registry Registry.
     * @param action Action of the row that had the focus; its match may be gone or full.
     */
    void restoreFocus(ecs::Registry& registry, std::string_view action) const;

    /** @brief Scroll list of the rows. */
    ecs::Entity _list = ecs::NULL_ENTITY;
    /** @brief "No match" line. */
    ecs::Entity _empty = ecs::NULL_ENTITY;
    /** @brief Scroll hint. */
    ecs::Entity _hint = ecs::NULL_ENTITY;
    /** @brief "RETOUR" button. */
    ecs::Entity _back = ecs::NULL_ENTITY;
    /** @brief Current rows. */
    std::vector<ecs::Entity> _rows;
    /** @brief Width of a row. */
    float _rowWidth = 0.0F;
};

/**
 * @brief Waiting for the answer to a create or join request; gives up after `MenuScenesPlugin::REQUEST_TIMEOUT`, or at
 *        once on back.
 */
class JoiningScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Joining".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Joining"; }

    /**
     * @brief Spawns the waiting screen and its timeout.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;
};

/** @brief In a match: no menu, the HUD and the arena are drawn by their plugins. */
class MatchScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Match".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Match"; }

    /**
     * @brief Clears the focus; Escape quits, as before the menus.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;
};

/** @brief End of a match, over the arena: victory or defeat, final score, back to the menu. */
class EndedScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Ended".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Ended"; }

    /**
     * @brief The arena stays visible behind.
     * @return `SceneMode::Overlay`.
     */
    [[nodiscard]] scene::SceneMode mode() const noexcept override { return scene::SceneMode::Overlay; }

    /**
     * @brief Spawns the result.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;
};

/** @brief Settings: volumes, saved at once, and the way to the controls screen. */
class SettingsScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Settings".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Settings"; }

    /**
     * @brief Spawns the sliders.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;
};

/** @brief Number of `engine::Key` values the controls screen tracks (`engine::Key` is 8 bits). */
inline constexpr std::size_t KEY_SLOTS = 256;

/** @brief Controls: a button per control showing its key; selecting one waits for the new key. */
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

    /** @brief Button of each control, indexed by `Control`. */
    std::array<ecs::Entity, CONTROL_COUNT> _buttons{};
    /** @brief Label of each button, indexed by `Control`. */
    std::array<ecs::Entity, CONTROL_COUNT> _labels{};
    /** @brief Control waiting for a key, if any. */
    std::optional<Control> _waiting;
    /** @brief Keys down at the previous frame, indexed by `engine::Key`, so only new presses count. */
    std::array<bool, KEY_SLOTS> _wasDown{};
    /** @brief Key just bound: clicks are ignored until it is released, so Enter or Space do not start a new wait. */
    engine::Key _justBound = engine::Key::Unknown;
};

}  // namespace rlss::client
