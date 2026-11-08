/**
 * @file RespawnPlugin.hpp
 * @brief Brings the destroyed R-Type ships back.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"

namespace rtype::server::gameplay {

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
 * @brief Brings back the ships of `ShipDestroyed`.
 *
 * A destroyed ship moves to its start position (`shipSpawnPoint()`) and gets `Respawning` and
 * `game::FLAG_INVULNERABLE` for `RESPAWN_DURATION` seconds (`ShipRespawnStarted`). Once the last respawn is over,
 * publishes `AllRespawnsOver`. A ship still inside the terrain when its respawn ends starts another one: its contact
 * began while it could not be hit, so no contact event would report it.
 */
class RespawnPlugin final : public engine::IPlugin {
  public:
    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeRespawnPlugin"; }

    /**
     * @brief Subscribes to the destroyed ships.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Counts down the respawns and makes the ships whose respawn is over vulnerable again.
     * @param engine Host engine.
     * @param time Fixed step time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the destroyed ships.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Moves a ship back to its start position, invulnerable for `RESPAWN_DURATION` seconds.
     * @param registry Room registry.
     * @param ship Player ship.
     */
    void respawn(ecs::Registry& registry, ecs::Entity ship);

    /** @brief Event bus of the room, set by `init()`. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Subscription to the destroyed ships. */
    std::optional<ecs::EventBus::EventToken> _destroyedToken;
    /** @brief True while at least one ship is respawning. */
    bool _respawning = false;
};

}  // namespace rtype::server::gameplay
