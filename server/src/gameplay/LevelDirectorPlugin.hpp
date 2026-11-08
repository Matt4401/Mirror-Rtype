/**
 * @file LevelDirectorPlugin.hpp
 * @brief Plays an R-Type level: spawns, scroll speed, pauses and restarts.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "level/Level.hpp"
#include "prefabs/PrefabParams.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Plays the level of a room with `timeline::TimelinePlugin`, the cursor moving at the scroll speed.
 *
 * - Publishes every event it plays as a `LevelEventPlayed`, for the plugins that handle `terrain` and `boss`. It
 *   handles the others: `spawn` creates the enemies, `scroll` changes the speed, and `end` restarts the level
 *   (`LevelStarted`).
 * - Once `startGame()` was called, the level is the whole game: `startGame()` publishes its `protocol::LevelStart`, and
 *   its `end` stops the scroll and publishes `LevelFinished` instead of restarting it. Nothing is played after that.
 * - Holds the scroll from `BossFightStarted` to `BossFightEnded` and from `ShipRespawnStarted` to `AllRespawnsOver`:
 *   it resumes, at its speed before the hold, once both are over. The events reached while it is held (in the same
 *   step as a boss) wait for it, so a `scroll` does not restart the scroll and an `end` does not restart the level
 *   during a fight. A `boss` event is never held: another boss joins the current fight.
 */
class LevelDirectorPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param level Level to play, shared by every room; `nullptr` plays no level (no enemy spawns).
     */
    explicit LevelDirectorPlugin(std::shared_ptr<const level::Level> level = nullptr);

    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeLevelDirectorPlugin"; }

    /**
     * @brief Requires `timeline::TimelinePlugin`, which plays the level.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the timeline and to the events that hold the scroll, and starts the level.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the bus before the engine tears it down.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Starts the game: publishes the `protocol::LevelStart` of the level, which the room sends to its players.
     *
     * Called when the room starts, after `GameStart` was sent. From then on, the level `end` finishes the game.
     */
    void startGame();

    /**
     * @brief Prefabs a level may use, per event type, and the parameters they read: the level catalog given to
     * `level::loadLevel`.
     *
     * A `spawn` event may name `alien_scout`, a `boss` event `boss_core`.
     *
     * @param type Event type.
     * @param prefab Prefab name.
     * @return The parameters the prefab reads beyond `x` and `y`, or nothing if `type` cannot use `prefab`.
     */
    [[nodiscard]] static std::optional<std::vector<std::string>> levelPrefab(std::string_view type,
                                                                             std::string_view prefab);

  private:
    /**
     * @brief Plays a level event, or keeps it until the scroll resumes.
     * @param event Event to play.
     */
    void play(const timeline::TimelineEvent& event);
    /** @brief Spawns the replicated enemy of a level `spawn` event. */
    void spawnEnemy(const prefabs::PrefabParams& params);
    /** @brief Puts the level at its start: events pending again, initial scroll speed. */
    void startLevel();
    /** @brief Stops the scroll, remembering its speed; does nothing if it is already held. */
    void holdScroll();
    /** @brief Resumes the scroll if nothing holds it any more, then plays the events that waited. */
    void releaseScroll();

    /** @brief Level played by the room, if any. */
    std::shared_ptr<const level::Level> _level;
    /** @brief Registry of the room, set by `init()`. */
    ecs::Registry* _registry = nullptr;
    /** @brief Event bus of the room, set by `init()`. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Plays the level, set by `init()`. */
    timeline::TimelinePlugin* _timeline = nullptr;
    /** @brief Subscriptions, removed by `shutdown()`. */
    std::vector<std::pair<ecs::EventBus::EventToken, void (*)(ecs::EventBus&, ecs::EventBus::EventToken)>> _tokens;
    /** @brief True once `startGame()` was called. */
    bool _gameStarted = false;
    /** @brief True once the level reached its `end` during a game: nothing is played any more. */
    bool _finished = false;
    /** @brief True during a boss fight. */
    bool _bossFight = false;
    /** @brief True while at least one ship is respawning. */
    bool _respawning = false;
    /** @brief True while the scroll is held, see `holdScroll()`. */
    bool _scrollHeld = false;
    /** @brief Scroll speed when the scroll was held, restored once it resumes. */
    float _speedBeforeHold = 0.0F;
    /** @brief Level events reached while the scroll is held, played once it resumes. */
    std::vector<timeline::TimelineEvent> _heldEvents;
};

}  // namespace rtype::server::gameplay
