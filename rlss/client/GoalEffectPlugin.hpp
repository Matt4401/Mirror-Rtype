/**
 * @file GoalEffectPlugin.hpp
 * @brief Client plugin blowing the ball up when a goal is scored.
 */

#pragma once

#include <optional>
#include <random>
#include <string_view>

#include "UiCanvas.hpp"
#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "game/Components.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Client: a short-lived piece of the goal explosion, moved, scaled and faded every frame by `GoalEffectPlugin`,
 * then removed.
 */
struct GoalParticle {
    /** @brief Seconds since it appeared. */
    float age = 0.0F;
    /** @brief Seconds it lasts. */
    float life = 1.0F;
    /** @brief Velocity, in uu/s. */
    float vx = 0.0F;
    /** @brief Velocity, in uu/s. */
    float vy = 0.0F;
    /** @brief Downward acceleration, in uu/s². */
    float gravity = 0.0F;
    /** @brief Share of the velocity lost per second (air drag). */
    float drag = 0.0F;
    /** @brief Size when it appears, in uu. */
    float startWidth = 0.0F;
    /** @brief Size when it appears, in uu. */
    float startHeight = 0.0F;
    /** @brief Size at the end of its life, in uu. */
    float endWidth = 0.0F;
    /** @brief Size at the end of its life, in uu. */
    float endHeight = 0.0F;
    /** @brief Size of its texture, in pixels (to turn sizes into scales). */
    float textureWidth = 1.0F;
    /** @brief Size of its texture, in pixels. */
    float textureHeight = 1.0F;
    /** @brief Opacity when it appears, in [0, 1]. */
    float startAlpha = 1.0F;
    /** @brief True if it fades out over its life; false keeps its opacity (an animated sprite fades by itself). */
    bool fades = true;
    /** @brief True if it turns to point where it flies (sparks). */
    bool alignToVelocity = false;
};

/**
 * @brief Blows the ball up when a goal is scored, like Rocket League's goal explosions.
 *
 * Watches the `game::MatchState` (written by `HudPlugin`). When the match enters the goal celebration, at the ball:
 * a fireball in the color of the team that scored (an animated strip played by the engine's `AnimationPlugin`), a
 * shockwave ring, a flash of light, sparks flying out and falling, smoke, and a flash of the whole screen. The server
 * takes the ball out of play during the celebration (and blows the cars around away): the ball is hidden until the
 * kickoff puts it back in the center.
 */
class GoalEffectPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "GoalEffectPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "GoalEffectPlugin"; }

    /**
     * @brief Declares the dependencies: assets, camera (ball entity), HUD (match state), ball view (its pieces).
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Reads the textures and the drawing service.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Starts an explosion on a new goal, moves the particles, hides or shows the ball, flashes the screen.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Removes the particles left.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Spawns the explosion.
     * @param engine Host engine.
     * @param x Center, in uu.
     * @param y Center, in uu.
     * @param blueScored True for the blue team's colors.
     */
    void explode(engine::Engine& engine, float x, float y, bool blueScored);

    /**
     * @brief Ages, moves and fades the particles; removes the ones whose life is over.
     * @param engine Host engine.
     * @param dt Frame time, in seconds.
     */
    static void stepParticles(engine::Engine& engine, float dt);

    /**
     * @brief Shows or hides every piece of the ball.
     * @param engine Host engine.
     * @param shown True to show it.
     */
    static void showBall(engine::Engine& engine, bool shown);

    /** @brief Match state of the previous frame. */
    std::optional<game::MatchState> _last;
    /** @brief Seconds the ball stays hidden after the celebration: the snapshots bring it back to the center later. */
    float _hiddenFor = 0.0F;
    /** @brief Seconds since the last goal, for the screen flash. */
    float _flashAge = 1.0e9F;
    /** @brief Color of the screen flash. */
    ecs::Rgba _flashColor;
    /** @brief Directions, speeds and sizes of the particles: every explosion looks a little different. */
    std::minstd_rand _random{std::random_device{}()};
    /** @brief Drawing service of the screen flash. */
    Typeface _screen;
    /** @brief Textures. */
    ecs::TextureId _explosionBlue = ecs::NO_TEXTURE;
    /** @brief Orange explosion strip. */
    ecs::TextureId _explosionOrange = ecs::NO_TEXTURE;
    /** @brief Shockwave ring. */
    ecs::TextureId _shockwave = ecs::NO_TEXTURE;
    /** @brief Spark streak. */
    ecs::TextureId _spark = ecs::NO_TEXTURE;
    /** @brief Soft glow. */
    ecs::TextureId _glow = ecs::NO_TEXTURE;
    /** @brief Smoke puff. */
    ecs::TextureId _puff = ecs::NO_TEXTURE;
};

}  // namespace rlss::client
