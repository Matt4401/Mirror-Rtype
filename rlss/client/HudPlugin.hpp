/**
 * @file HudPlugin.hpp
 * @brief Client plugin drawing the match HUD: score, clock, kickoff countdown, goal banner and boost meter.
 */

#pragma once

#include <optional>
#include <string_view>

#include "UiCanvas.hpp"
#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "game/Components.hpp"
#include "tools/cvars/CVarRef.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Keeps the state of the joined match and draws the HUD on screen, through `engine::IScreenDrawService`.
 *
 * Each `MatchStatus` of the joined match is written into the `game::MatchState` of a HUD entity, where other plugins
 * read it (the sounds). Every frame, while in a match, it draws:
 * - at the top, the score: the blue score on a blue panel, the clock (gold "+0:42" in overtime), the orange score;
 * - during the kickoff countdown, the big 3, 2, 1, then "GO!";
 * - after a goal, a "GOAL!" banner in the color of the team that scored;
 * - in the bottom-right corner, the boost meter of the local car (a ring of ticks and the amount, 0 to 100).
 *
 * Nothing is drawn in the menus nor once the match is over (the end screen shows the score). Sizes follow the window
 * height (designed for 1080 pixels).
 */
class HudPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "HudPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "HudPlugin"; }

    /**
     * @brief Declares the dependencies: assets, camera (local car), network.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Spawns the HUD entity and subscribes to the match status and to `MatchJoined` / `MatchLeft`.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Draws the HUD of the frame.
     * @param engine Host engine.
     * @param time Frame time (animations).
     */
    void lateUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Replaces the match state, and starts the animations of what changed.
     * @param engine Host engine.
     * @param state New state.
     */
    void setState(engine::Engine& engine, const game::MatchState& state);

    /** @brief Draws the score and the clock at the top. */
    void drawScore(float width, float scale) const;
    /** @brief Draws the kickoff countdown, "GO!" and the goal banner. */
    void drawAnnouncements(float width, float height, float scale) const;
    /** @brief Draws the boost meter of the local car. */
    void drawBoost(engine::Engine& engine, float width, float height, float scale, float dt);

    /** @brief Entity carrying the `game::MatchState`. */
    ecs::Entity _hud = 0;
    /** @brief State shown (`NO_MATCH` in the menus). */
    game::MatchState _state{.phase = game::NO_MATCH};
    /** @brief True between `MatchJoined` and `MatchLeft`. */
    bool _inMatch = false;
    /** @brief Seconds since the countdown value last changed. */
    float _countdownTime = 0.0F;
    /** @brief Seconds since play resumed after a countdown ("GO!"), or a large value. */
    float _goTime = 1.0e9F;
    /** @brief Seconds since the goal celebration began. */
    float _goalTime = 0.0F;
    /** @brief True when the blue team scored the goal being celebrated. */
    bool _blueScored = true;
    /** @brief Boost amount shown, easing toward the real one. */
    float _shownBoost = 0.0F;
    /** @brief Font and drawing service. */
    Typeface _typeface;
    /** @brief `cl_hud`: whether the HUD is drawn; unset without a console. */
    tools::CVarRef<bool> _visible;
    /** @brief Textures of the HUD. */
    ecs::TextureId _disc = ecs::NO_TEXTURE;
    /** @brief One tick of the boost ring. */
    ecs::TextureId _tick = ecs::NO_TEXTURE;
    /** @brief Score panel slanted on the left. */
    ecs::TextureId _slantLeft = ecs::NO_TEXTURE;
    /** @brief Score panel slanted on the right. */
    ecs::TextureId _slantRight = ecs::NO_TEXTURE;
    /** @brief Soft band behind the banners. */
    ecs::TextureId _band = ecs::NO_TEXTURE;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _left;
};

}  // namespace rlss::client
