/**
 * @file CombatPlugin.hpp
 * @brief R-Type combat rules: player shots, hits, kills, scores and power-ups.
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
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "gameplay/GameplayEvents.hpp"
#include "net/ReplicationEvents.hpp"
#include "protocol/Messages.hpp"

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

/**
 * @brief Combat rules of a room.
 *
 * - Fires a `player_bullet` from a ship while its input holds `game::ACTION_SHOOT`, at most every `SHOT_COOLDOWN`
 *   (`RAPID_FIRE_COOLDOWN` for `RAPID_FIRE_DURATION` seconds after picking up a power-up). The bullet's
 *   `EntityNetState::subtype` is the index of the player who fired it.
 * - Turns physics contacts into R-Type events with a `collision::CollisionRouter`. A player shot touching an enemy
 *   hurts it: an enemy with a `prefabs::Health` loses one point (its `EntityNetState::health` follows) and dies at 0,
 *   the others die on the first hit, and the kill gives `POINTS_PER_KILL` to the shooter.
 * - An enemy shot, an enemy or an obstacle touching a ship that is not `Respawning` publishes `ShipDestroyed`.
 * - Logs the score of each player every `SCORE_LOG_INTERVAL` seconds of game time.
 * - On `LevelFinished`, publishes the `protocol::LevelComplete` with the scores, then `protocol::GameOver{Victory}`
 *   after the last level. The room sends them to its players: the plugin itself knows nothing about the network.
 */
class CombatPlugin final : public engine::IPlugin {
  public:
    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeCombatPlugin"; }

    /**
     * @brief Requires `systems::PhysicsPlugin`, which publishes the contacts.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the inputs and to the collision events, and creates the collision router.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Fires the pending shots, counts the cooldowns down and logs the scores when due.
     * @param engine Host engine.
     * @param time Fixed step time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the bus before the engine tears it down.
     * @param engine Host engine.
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
    /** @brief Applies a shot: a player shot hurts the enemy it hit, an enemy shot destroys the ship it hit. */
    void onShotHit(const collision::ShotHitEvent& hit);
    /** @brief Destroys a ship touching an enemy or an obstacle. */
    void onShipContact(const collision::ShipContactEvent& contact);
    /** @brief Picks up a power-up touched by a ship: the power-up disappears and its player gets rapid fire. */
    void onPowerUpTouched(const collision::PowerUpTouchedEvent& touch);
    /**
     * @brief Tells whether a ship can be destroyed now: alive, and not respawning.
     * @param ship Player ship.
     * @return True if a hit destroys it.
     */
    [[nodiscard]] bool isVulnerable(ecs::Entity ship) const;
    /**
     * @brief Applies a player shot to an enemy.
     * @param target Enemy hit.
     * @return True if the enemy dies: no `prefabs::Health`, or its last point.
     */
    bool takeHit(ecs::Entity target);
    /** @brief Spawns a replicated player bullet. */
    void fire(const PendingShot& shot);
    /** @brief Publishes the protocol messages of a finished level, with the scores. */
    void onLevelFinished(const LevelFinished& finished);
    /** @brief Scores of the players of the game, in player order. */
    [[nodiscard]] std::vector<std::uint32_t> playerScores() const;
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
    /** @brief Subscription to the finished levels. */
    std::optional<ecs::EventBus::EventToken> _levelToken;
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
    /** @brief Seconds left before the next score log. */
    float _scoreLogTimer = SCORE_LOG_INTERVAL;
};

}  // namespace rtype::server::gameplay
