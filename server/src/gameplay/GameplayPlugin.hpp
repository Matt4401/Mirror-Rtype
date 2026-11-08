/**
 * @file GameplayPlugin.hpp
 * @brief Server-side R-Type rules of a room: enemy waves, player shots, kills and scores.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "level/Level.hpp"
#include "net/ReplicationEvents.hpp"
#include "prefabs/PrefabParams.hpp"
#include "protocol/Messages.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace rtype::server::gameplay {

/** @brief Seconds between two shots of the same player while the shoot button is held. */
inline constexpr float SHOT_COOLDOWN = 0.2F;
/** @brief Seconds of game time between two score logs. */
inline constexpr float SCORE_LOG_INTERVAL = 5.0F;
/** @brief Points given to a player for each enemy their shots destroy. */
inline constexpr std::uint32_t POINTS_PER_KILL = 1;
/** @brief Seconds of rapid fire given by a power-up. */
inline constexpr float RAPID_FIRE_DURATION = 5.0F;
/** @brief Seconds between two shots of a player with rapid fire. */
inline constexpr float RAPID_FIRE_COOLDOWN = 0.08F;
/** @brief Seconds a destroyed ship blinks after coming back: it cannot be hit, and the level waits for it. */
inline constexpr float RESPAWN_DURATION = 3.0F;
/** @brief Abscissa where the ships start and respawn. */
inline constexpr float SHIP_SPAWN_X = 200.0F;
/** @brief Ordinate where the first player's ship starts and respawns. */
inline constexpr float SHIP_SPAWN_FIRST_Y = 120.0F;
/** @brief Vertical gap between the start positions of two players. */
inline constexpr float SHIP_SPAWN_SPACING_Y = 160.0F;

/** @brief Position where a ship starts and respawns. */
struct ShipSpawnPoint {
    /** @brief Abscissa. */
    float x = SHIP_SPAWN_X;
    /** @brief Ordinate. */
    float y = SHIP_SPAWN_FIRST_Y;
};

/**
 * @brief Gives the start position of a player's ship, one row per player.
 * @param playerIndex Index of the player, 0 for the first one.
 * @return Where the ship starts and respawns.
 */
[[nodiscard]] constexpr ShipSpawnPoint shipSpawnPoint(const std::uint8_t playerIndex) noexcept {
    return {.x = SHIP_SPAWN_X, .y = SHIP_SPAWN_FIRST_Y + (SHIP_SPAWN_SPACING_Y * static_cast<float>(playerIndex))};
}

/**
 * @brief R-Type rules of one room, run by the room's engine.
 *
 * - Plays its level with `timeline::TimelinePlugin`, the cursor moving at the scroll speed: `spawn` events create
 *   the enemies, `scroll` events change the speed, and the `end` event restarts the level (until there are several
 *   levels).
 * - A `boss` event stops the scroll and spawns a `boss_core`, driven by the event's Wren `script` if any (module
 *   `scripts/Boss` for `scripts/Boss.wren`, class named after the file unless the event sets `class`). The boss flies
 *   in until its `stop_x`; once no boss is left, the scroll resumes at the speed it had before the fight.
 * - A `terrain` event starts a section along the top or bottom edge. It is built column by column
 *   (`game::TERRAIN_BLOCK_WIDTH` wide, `KIND_OBSTACLE`) as it enters the screen, and the columns move left at the
 *   scroll speed, so they stop during a boss fight. A ship overlapping a column is destroyed; player shots stop on it.
 * - A destroyed ship respawns at its start position (`shipSpawnPoint()`) with `game::FLAG_INVULNERABLE` for
 *   `RESPAWN_DURATION` seconds: it cannot be hit, and the scroll is held like during a boss fight.
 * - An enemy with a `prefabs::Health` loses one point per player shot and dies at 0; its `EntityNetState::health`
 *   follows. The other enemies die on the first hit.
 * - Fires a `player_bullet` from a ship while its input holds `game::ACTION_SHOOT`, at most every `SHOT_COOLDOWN`.
 *   The bullet's `EntityNetState::subtype` is the index of the player who fired it.
 * - Turns physics contacts into R-Type events with a `collision::CollisionRouter`; a player shot touching an enemy
 *   destroys both and gives `POINTS_PER_KILL` to its shooter. An enemy shot or an enemy touching a ship destroys the
 *   ship (it respawns). A power-up touched by a ship is picked up: its player fires every `RAPID_FIRE_COOLDOWN` for
 *   `RAPID_FIRE_DURATION` seconds.
 * - Destroys bullets and enemies that left the screen.
 * - Logs the score of each player every `SCORE_LOG_INTERVAL` seconds of game time.
 *
 * Every spawned entity is replicated, so the clients only draw what the server simulates.
 */
class GameplayPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin; scores start at 0.
     * @param level Level to play, shared by every room; `nullptr` plays no level (no enemy spawns).
     */
    explicit GameplayPlugin(std::shared_ptr<const level::Level> level = nullptr);
    /** @brief Destructor. */
    ~GameplayPlugin() override = default;
    /** @brief Deleted copy constructor. */
    GameplayPlugin(const GameplayPlugin&) = delete;
    /** @brief Deleted copy assignment operator. */
    GameplayPlugin& operator=(const GameplayPlugin&) = delete;
    /** @brief Deleted move constructor. */
    GameplayPlugin(GameplayPlugin&&) = delete;
    /** @brief Deleted move assignment operator. */
    GameplayPlugin& operator=(GameplayPlugin&&) = delete;

    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeGameplayPlugin"; }

    /**
     * @brief Requires `systems::PhysicsPlugin`, which publishes the contacts, and `timeline::TimelinePlugin`, which
     * plays the level.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the inputs and to the level events, creates the collision router and starts the level.
     * @param engine Engine of the room.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Fires the pending shots, removes off-screen entities and logs the scores when due.
     * @param engine Engine of the room.
     * @param time Fixed step timing.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the bus before the engine tears it down.
     * @param engine Engine of the room.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Sets how many players the game has, so that their scores are logged. Called when the room starts.
     * @param playerCount Number of players, at most `protocol::MAX_PLAYERS`; larger values are clamped.
     */
    void setPlayerCount(std::size_t playerCount) noexcept;

    /**
     * @brief Score of a player.
     * @param playerIndex Index of the player, as in `GameStart::playerIndex`.
     * @return Its score; 0 for an index out of range.
     */
    [[nodiscard]] std::uint32_t score(std::size_t playerIndex) const noexcept;

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
    /** @brief A shot requested by an input, fired at the next fixed step. */
    struct PendingShot {
        /** @brief Index of the player who fires. */
        std::uint8_t playerIndex = 0;
        /** @brief Position of the ship when it fired. */
        float x = 0.0F;
        /** @brief Position of the ship when it fired. */
        float y = 0.0F;
    };

    /** @brief Queues a shot if the input holds the shoot action and the player's cooldown is over. */
    void onInput(const ecs::net::InputApplied& event);
    /** @brief Applies a shot: a player shot hurts the enemy it hit and scores the kill, an enemy shot destroys the ship
     * it hit. */
    void onShotHit(const collision::ShotHitEvent& hit);
    /** @brief Destroys a ship touching an enemy or an obstacle. */
    void onShipContact(const collision::ShipContactEvent& contact);
    /** @brief Picks up a power-up touched by a ship: the power-up disappears and its player gets rapid fire. */
    void onPowerUpTouched(const collision::PowerUpTouchedEvent& touch);
    /** @brief Spawns a replicated player bullet. */
    void fire(const PendingShot& shot);
    /** @brief Handles a level event: spawn, scroll speed change or end of level. */
    void onLevelEvent(const timeline::TimelineEventFired& fired);
    /**
     * @brief Plays a level event, or keeps it until the scroll resumes.
     *
     * The scroll is held during a boss fight, so only the events reached in the same step as the boss can fire then.
     * They come after the boss in the level, so they wait for it: a `scroll` must not restart the scroll and an `end`
     * must not restart the level while the boss is alive. Another `boss` joins the current fight.
     *
     * @param event Event to play.
     */
    void playLevelEvent(const timeline::TimelineEvent& event);
    /** @brief Spawns the replicated entity of a level `spawn` event. */
    void spawnFromLevel(const prefabs::PrefabParams& params);
    /** @brief A terrain section still entering the screen: what is left to build. */
    struct TerrainSection {
        /** @brief Center ordinate of its columns. */
        float y = 0.0F;
        /** @brief Height of its columns, a multiple of `game::TERRAIN_HEIGHT_UNIT`. */
        float height = 0.0F;
        /** @brief Columns left to build. */
        std::size_t columnsLeft = 0;
        /** @brief Last column built, to place the next one right after it; none before the first. */
        std::optional<ecs::Entity> lastColumn;
    };

    /** @brief Handles a `terrain` event: queues a section, whose columns enter from the right edge. */
    void startTerrain(const prefabs::PrefabParams& params);
    /** @brief Builds the columns entering the screen, moves every column at the scroll speed. */
    void updateTerrain();
    /**
     * @brief Tells whether a ship can be destroyed now: alive, and not respawning.
     * @param ship Player ship.
     * @return True if a hit destroys it.
     */
    [[nodiscard]] bool isVulnerable(ecs::Entity ship) const;
    /** @brief Destroys the ships overlapping a terrain column, unless they are respawning. */
    void crashShipsIntoTerrain();
    /**
     * @brief Destroys a ship: it comes back at its start position, invulnerable, and the scroll is held.
     * @param ship Player ship.
     */
    void destroyShip(ecs::Entity ship);
    /**
     * @brief Counts down the respawns, and makes the ships whose respawn is over vulnerable again.
     * @param deltaTime Seconds since the previous step.
     */
    void updateRespawns(float deltaTime);
    /** @brief Handles a `boss` event: stops the scroll and spawns the replicated, possibly scripted, boss. */
    void startBossFight(const prefabs::PrefabParams& params);
    /** @brief Stops the bosses that reached their place, and ends the fight once no boss is left. */
    void updateBossFight();
    /** @brief Stops the scroll, remembering its speed; does nothing if it is already held. */
    void holdScroll();
    /**
     * @brief Resumes the scroll once nothing holds it any more, then plays the level events that waited.
     *
     * Several things may hold the scroll at once (a boss fight, a respawning ship): it only resumes when all of them
     * are over.
     */
    void updateScrollHold();
    /**
     * @brief Applies a player shot to an enemy.
     * @param target Enemy hit.
     * @return True if the enemy dies: no `prefabs::Health`, or its last point.
     */
    bool takeHit(ecs::Entity target);
    /** @brief Puts the level at its start: events pending again, initial scroll speed. */
    void startLevel();
    /** @brief Kills the player bullets past the right edge, and the enemies, enemy shots and power-ups past the left
     * one. */
    void killOffscreenEntities();
    /** @brief Writes the scores in the server log. */
    void logScores() const;

    /** @brief Registry of the room, set by `init()`. */
    ecs::Registry* _registry = nullptr;
    /** @brief Event bus of the room, set by `init()`. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Turns the physics contacts into R-Type events. */
    std::unique_ptr<collision::CollisionRouter> _router;
    /** @brief Subscription to the applied inputs. */
    std::optional<ecs::EventBus::EventToken> _inputToken;
    /** @brief Subscription to the shot hits. */
    std::optional<ecs::EventBus::EventToken> _hitToken;
    /** @brief Subscription to the ship contacts. */
    std::optional<ecs::EventBus::EventToken> _contactToken;
    /** @brief Subscription to the power-ups touched. */
    std::optional<ecs::EventBus::EventToken> _powerUpToken;
    /** @brief Shots requested since the last fixed step. */
    std::vector<PendingShot> _pendingShots;
    /** @brief Seconds before each player can fire again. */
    std::array<float, protocol::MAX_PLAYERS> _shotCooldowns{};
    /** @brief Seconds of rapid fire left for each player. */
    std::array<float, protocol::MAX_PLAYERS> _rapidFire{};
    /** @brief Score of each player. */
    std::array<std::uint32_t, protocol::MAX_PLAYERS> _scores{};
    /** @brief Number of players whose score is logged. */
    std::size_t _playerCount = 0;
    /** @brief Level played by the room, if any. */
    std::shared_ptr<const level::Level> _level;
    /** @brief Plays the level, set by `init()`. */
    timeline::TimelinePlugin* _timeline = nullptr;
    /** @brief Subscription to the level events. */
    std::optional<ecs::EventBus::EventToken> _levelToken;
    /** @brief Terrain sections still entering the screen. */
    std::vector<TerrainSection> _terrain;
    /** @brief True from a `boss` event until no boss is left. */
    bool _bossFight = false;
    /** @brief True while at least one ship is respawning. */
    bool _respawning = false;
    /** @brief True while the scroll is stopped by the game, see `holdScroll()`. */
    bool _scrollHeld = false;
    /** @brief Scroll speed when the scroll was held, restored once it resumes. */
    float _speedBeforeHold = 0.0F;
    /** @brief Level events fired while the scroll is held (e.g. in the same step as a boss), played once it resumes. */
    std::vector<timeline::TimelineEvent> _heldEvents;
    /** @brief Seconds left before the next score log. */
    float _scoreLogTimer = SCORE_LOG_INTERVAL;
};

}  // namespace rtype::server::gameplay
