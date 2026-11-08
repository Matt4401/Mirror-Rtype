/**
 * @file CleanupPlugin.hpp
 * @brief Destroys the R-Type entities that left the screen.
 */

#pragma once

#include <string_view>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "ecs/time/Time.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Destroys, on each fixed step, the player bullets past the right edge, and the enemies, enemy shots,
 * power-ups and terrain blocks past the left one (`OFFSCREEN_MARGIN` away).
 */
class CleanupPlugin final : public engine::IPlugin {
  public:
    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeCleanupPlugin"; }

    /**
     * @brief Destroys the entities that left the screen.
     * @param engine Host engine.
     * @param time Fixed step time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;
};

}  // namespace rtype::server::gameplay
