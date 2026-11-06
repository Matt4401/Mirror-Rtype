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
#include <string_view>
#include <vector>

#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "net/ReplicationEvents.hpp"
#include "protocol/Messages.hpp"

namespace rtype::server::gameplay {

/** @brief Seconds between two enemy spawns. */
inline constexpr float ENEMY_SPAWN_INTERVAL = 1.2F;
/** @brief Seconds between two shots of the same player while the shoot button is held. */
inline constexpr float SHOT_COOLDOWN = 0.2F;
/** @brief Seconds of game time between two score logs. */
inline constexpr float SCORE_LOG_INTERVAL = 5.0F;
/** @brief Points given to a player for each enemy their shots destroy. */
inline constexpr std::uint32_t POINTS_PER_KILL = 1;

/**
 * @brief R-Type rules of one room, run by the room's engine.
 *
 * - Spawns an `alien_scout` every `ENEMY_SPAWN_INTERVAL` seconds, at heights cycled through deterministically.
 * - Fires a `player_bullet` from a ship while its input holds `game::ACTION_SHOOT`, at most every `SHOT_COOLDOWN`.
 *   The bullet's `EntityNetState::subtype` is the index of the player who fired it.
 * - Turns physics contacts into R-Type events with a `collision::CollisionRouter`; a player shot touching an enemy
 *   destroys both and gives `POINTS_PER_KILL` to its shooter.
 * - Destroys bullets and enemies that left the screen.
 * - Logs the score of each player every `SCORE_LOG_INTERVAL` seconds of game time.
 *
 * Every spawned entity is replicated, so the clients only draw what the server simulates.
 */
class GameplayPlugin final : public engine::IPlugin {
  public:
    /** @brief Creates the plugin; scores start at 0. */
    GameplayPlugin() = default;
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
     * @brief Requires `systems::PhysicsPlugin`, which publishes the contacts.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the inputs and creates the collision router.
     * @param engine Engine of the room.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Fires the pending shots, spawns enemies, removes off-screen entities and logs the scores when due.
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
    /** @brief Destroys the shot and the enemy it hit, and scores the kill. */
    void onShotHit(const collision::ShotHitEvent& hit);
    /** @brief Spawns a replicated player bullet. */
    void fire(const PendingShot& shot);
    /** @brief Spawns a replicated enemy at the next spawn height. */
    void spawnEnemy();
    /** @brief Kills the bullets and enemies outside the screen. */
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
    /** @brief Shots requested since the last fixed step. */
    std::vector<PendingShot> _pendingShots;
    /** @brief Seconds before each player can fire again. */
    std::array<float, protocol::MAX_PLAYERS> _shotCooldowns{};
    /** @brief Score of each player. */
    std::array<std::uint32_t, protocol::MAX_PLAYERS> _scores{};
    /** @brief Number of players whose score is logged. */
    std::size_t _playerCount = 0;
    /** @brief Seconds left before the next enemy spawns. */
    float _enemySpawnTimer = ENEMY_SPAWN_INTERVAL;
    /** @brief Number of enemies spawned so far, used to pick the next spawn height. */
    std::size_t _enemiesSpawned = 0;
    /** @brief Seconds left before the next score log. */
    float _scoreLogTimer = SCORE_LOG_INTERVAL;
};

}  // namespace rtype::server::gameplay
