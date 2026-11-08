/**
 * @file EnemyFirePlugin.hpp
 * @brief Makes the R-Type enemies with a gun fire at the ships.
 */

#pragma once

#include <string_view>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "ecs/time/Time.hpp"
#include "gameplay/Targeting.hpp"

namespace rtype::server::gameplay {

/** @brief Distance in front of an enemy at which its missiles appear, in pixels. */
inline constexpr float MUZZLE_OFFSET = 24.0F;

/**
 * @brief Counts down the `prefabs::EnemyGun` of the enemies and fires their missiles, on each fixed step.
 *
 * An enemy only fires while it is on the screen: it does not shoot the ships from outside. Each shot is a replicated
 * missile towards the nearest ship that can be hit (`nearestShip()`), or straight ahead without one; a
 * `prefabs::GunPattern::Spread` gun adds two shots, `prefabs::SPREAD_ANGLE` on each side.
 */
class EnemyFirePlugin final : public engine::IPlugin {
  public:
    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeEnemyFirePlugin"; }

    /**
     * @brief Counts the guns down and fires those that are ready.
     * @param engine Host engine.
     * @param time Fixed step time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

  private:
    /** @brief A shot decided while walking the guns, fired once the walk is over. */
    struct Shot {
        /** @brief Where the missile appears. */
        AimPoint from;
        /** @brief Its velocity. */
        Velocity velocity;
    };

    /** @brief Shots of the current step; capacity kept between steps. */
    std::vector<Shot> _shots;
};

}  // namespace rtype::server::gameplay
