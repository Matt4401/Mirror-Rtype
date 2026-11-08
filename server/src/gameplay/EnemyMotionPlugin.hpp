/**
 * @file EnemyMotionPlugin.hpp
 * @brief Moves the R-Type enemies that follow a pattern instead of a straight line.
 */

#pragma once

#include <string_view>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "ecs/time/Time.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Moves the enemies with a `prefabs::SineWave` up and down on each fixed step, around the height they appeared
 * at. Their horizontal move stays their velocity.
 */
class EnemyMotionPlugin final : public engine::IPlugin {
  public:
    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeEnemyMotionPlugin"; }

    /**
     * @brief Advances the waves.
     * @param engine Host engine.
     * @param time Fixed step time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;
};

}  // namespace rtype::server::gameplay
