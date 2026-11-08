/**
 * @file ArenaViewPlugin.hpp
 * @brief Client plugin drawing the arena.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"

namespace ecs::time {
struct Time;
}  // namespace ecs::time

namespace rlss::client {

/** @brief Thickness of the walls drawn around the playing area, in uu. */
inline constexpr float ARENA_WALL = 40.0F;

/**
 * @brief Draws the arena (`rlss::arena`) and the stadium around it.
 *
 * The arena is a picture drawn by `rlss/assets/generate_textures.py` from the same dimensions (walls, glass, grass,
 * goals tinted with the color of the team defending them), cut in tiles. Through its glass, the stadium (stands,
 * floodlights) moves slower than the arena when the camera moves: a parallax that gives the scene depth.
 */
class ArenaViewPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "ArenaViewPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ArenaViewPlugin"; }

    /**
     * @brief Declares the dependency on `AssetsPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the stadium and the arena tiles.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Moves the stadium with the final camera of the frame (parallax).
     * @param engine Host engine.
     * @param time Frame time (unused).
     */
    void lateUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

  private:
    /** @brief Stadium entity, 0 without its texture. */
    ecs::Entity _stadium = 0;
};

}  // namespace rlss::client
