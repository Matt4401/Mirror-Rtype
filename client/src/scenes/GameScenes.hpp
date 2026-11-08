/**
 * @file GameScenes.hpp
 * @brief Scenes of an R-Type game: the game and its HUD, the pause menu, the game over screen.
 */

#pragma once

#include <string>
#include <string_view>

#include "ecs/Entity.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneTypes.hpp"

namespace client {

/** @brief Name of the scripted overlay showing the level banners (`scripts/scenes/LevelTransition.wren`). */
inline constexpr std::string_view LEVEL_TRANSITION_SCENE = "LevelTransition";

/** @brief Registry resource: text of the next level banner, read by the `level_banner` prefab. */
struct LevelBanner {
    /** @brief Main line ("LEVEL 2"). */
    std::string title;
    /** @brief Second line (level name, score). */
    std::string subtitle;
};

/**
 * @brief The game: HUD (level, score, player), level banners, and the way to the pause menu.
 *
 * The world itself (ships, enemies, shots) comes from the replication and is drawn by `RTypeGamePlugin`.
 * Each `LevelStart` and `LevelComplete` shows a banner in the scripted `LevelTransition` overlay. The game ends on
 * `GameEnded` (game over screen), on leaving (lobby), or when the connection is lost (connect screen).
 */
class GameScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Game".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Game"; }

    /**
     * @brief Spawns the HUD and shows the level already announced, if any.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /**
     * @brief Shows a banner in the level transition overlay.
     * @param context Scene context.
     * @param title Main line.
     * @param subtitle Second line.
     */
    static void showBanner(scene::SceneContext& context, std::string title, std::string subtitle);

    /** @brief Level line of the HUD. */
    ecs::Entity _level = ecs::NULL_ENTITY;
    /** @brief Score line of the HUD. */
    ecs::Entity _score = ecs::NULL_ENTITY;
};

/**
 * @brief Pause menu over the game: local only, the game goes on for the other players and on the server.
 *
 * While it is open, the local ship receives no input (`PlayerControlPlugin::setPaused`).
 */
class PauseScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Pause".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Pause"; }

    /**
     * @brief Drawn over the game, which keeps running.
     * @return `SceneMode::Overlay`.
     */
    [[nodiscard]] scene::SceneMode mode() const noexcept override { return scene::SceneMode::Overlay; }

    /**
     * @brief Spawns the menu and pauses the local inputs.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

    /**
     * @brief Resumes the local inputs.
     * @param context Scene context.
     */
    void onExit(scene::SceneContext& context) override;

    /**
     * @brief Focuses the first button again.
     * @param context Scene context.
     */
    void onResume(scene::SceneContext& context) override;

  private:
    /** @brief "Resume" button. */
    ecs::Entity _resume = ecs::NULL_ENTITY;
};

/** @brief End of a game: victory or defeat, scores, back to the lobby. */
class GameOverScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "GameOver".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "GameOver"; }

    /**
     * @brief Spawns the result of the last game.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;
};

}  // namespace client
