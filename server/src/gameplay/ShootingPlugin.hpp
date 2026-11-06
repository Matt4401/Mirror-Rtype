#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "input/ActionId.hpp"

namespace rtype::server {

/** @brief Button action of a ship: its id is in `NetInput::held` while the player holds it. */
inline constexpr input::ActionId ACTION_SHOOT = input::makeActionId("shoot");
/** @brief Minimum time between two shots of a ship, in seconds. */
inline constexpr float SHOT_INTERVAL = 0.2F;
/** @brief Where a bullet appears, in world units in front of the ship. */
inline constexpr float SHOT_OFFSET_X = 40.0F;
/** @brief X position past which a bullet is off screen and removed. */
inline constexpr float BULLET_MAX_X = 1344.0F;

/**
 * @brief Time before a ship can shoot again. An entity without it cannot shoot.
 *
 * The entity must also carry `ecs::Transform` and `game::EntityNetState`.
 */
struct ShotCooldown {
    /** @brief Remaining time in seconds; the ship can shoot at 0. */
    float remaining = 0.0F;
};

/**
 * @brief Server side of the shots: the client only says that the button is held, and receives the bullets in snapshots.
 *
 * The rate is decided here, so a modified client cannot shoot faster, whatever the inputs it sends.
 */
class ShootingPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "ShootingPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ShootingPlugin"; }

    /**
     * @brief Declares the dependencies on the replication, which applies the inputs, and on the physics, which moves
     * the bullets.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the applied inputs.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Makes the cooldowns elapse and removes the off-screen bullets.
     * @param engine Host engine.
     * @param time Time of this step.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the applied inputs.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Subscription to `InputApplied`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _inputs;
};

}  // namespace rtype::server
