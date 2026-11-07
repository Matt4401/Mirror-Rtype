/**
 * @file ArenaViewPlugin.hpp
 * @brief Client plugin drawing the arena and framing it with the camera.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace rlss::client {

/**
 * @brief Draws the arena (`rlss::arena`) once with flat colored rectangles: backdrop, walls with rounded corners, grass
 * floor, center line and circle, and the two goals (tinted with the color of the team defending them). Adds a camera
 * that fits the whole arena in the window.
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
     * @brief Spawns the arena shapes and the camera.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;
};

}  // namespace rlss::client
