/**
 * @file ArenaViewPlugin.hpp
 * @brief Client plugin drawing the arena.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace rlss::client {

/** @brief Thickness of the walls drawn around the playing area, in uu. */
inline constexpr float ARENA_WALL = 40.0F;

/**
 * @brief Draws the arena (`rlss::arena`) once with flat colored rectangles: backdrop, walls with rounded corners, grass
 * floor, center line and circle, and the two goals (tinted with the color of the team defending them). The camera is
 * `CameraPlugin`'s.
 */
class ArenaViewPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "ArenaViewPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ArenaViewPlugin"; }

    /**
     * @brief Declares the dependency on `render::RenderPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the arena shapes.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;
};

}  // namespace rlss::client
