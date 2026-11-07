#include "HudPlugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <tuple>
#include <variant>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "UiCanvas.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/Components.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::client {

namespace {

/** @brief The HUD is laid out for a window this tall, and scaled with the window height. */
constexpr float DESIGN_HEIGHT = 1080.0F;
constexpr float MIN_SCALE = 0.55F;
constexpr float MAX_SCALE = 2.0F;

constexpr int PHASE_PLAYING = static_cast<int>(protocol::MatchPhase::Playing);
constexpr int PHASE_GOAL = static_cast<int>(protocol::MatchPhase::Goal);
constexpr int PHASE_KICKOFF = static_cast<int>(protocol::MatchPhase::Kickoff);
constexpr int PHASE_ENDED = static_cast<int>(protocol::MatchPhase::Ended);

// Score bug, in design pixels.
constexpr float SCORE_TOP = 18.0F;
constexpr float SCORE_HEIGHT = 62.0F;
constexpr float CLOCK_WIDTH = 168.0F;
constexpr float PANEL_WIDTH = 132.0F;
constexpr float SCORE_CAP = 32.0F;
constexpr float CLOCK_CAP = 28.0F;
constexpr float LABEL_CAP = 12.0F;
constexpr float SHADOW = 3.0F;

// Announcements.
constexpr float COUNTDOWN_CAP = 150.0F;
constexpr float COUNTDOWN_CENTER = 0.32F;
constexpr float GOAL_CAP = 120.0F;
constexpr float GOAL_SUB_CAP = 26.0F;
constexpr float BANNER_CENTER = 0.3F;
constexpr float BANNER_WIDTH = 1500.0F;
constexpr float BANNER_HEIGHT = 250.0F;
/** @brief Seconds a popping text takes to settle to its size. */
constexpr float POP_TIME = 0.25F;
/** @brief A popping text starts this much bigger. */
constexpr float POP_GROWTH = 0.6F;
/** @brief Seconds "GO!" stays on screen. */
constexpr float GO_TIME = 0.9F;

// Boost meter.
constexpr float BOOST_RADIUS = 92.0F;
constexpr float BOOST_MARGIN = 34.0F;
constexpr int BOOST_TICKS = 36;
/** @brief The ring is open at the bottom: it runs clockwise from START over SWEEP radians. */
constexpr float BOOST_START = 0.75F * std::numbers::pi_v<float>;
constexpr float BOOST_SWEEP = 1.5F * std::numbers::pi_v<float>;
constexpr float TICK_WIDTH = 9.0F;
constexpr float TICK_LENGTH = 24.0F;
constexpr float BOOST_CAP = 50.0F;
/** @brief Rate (1/s) at which the shown amount catches up with the real one. */
constexpr float BOOST_EASING = 14.0F;

/** @brief Source region selecting a whole texture. */
constexpr ecs::TextureRect WHOLE{};

constexpr ecs::Rgba WHITE{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr ecs::Rgba SHADOW_COLOR{.r = 0, .g = 0, .b = 0, .a = 120};
constexpr ecs::Rgba CLOCK_PANEL{.r = 10, .g = 14, .b = 30, .a = 225};
constexpr ecs::Rgba CLOCK_EDGE{.r = 255, .g = 255, .b = 255, .a = 40};
constexpr ecs::Rgba GOLD{.r = 255, .g = 206, .b = 64, .a = 255};
constexpr ecs::Rgba DIM{.r = 170, .g = 182, .b = 205, .a = 255};
constexpr ecs::Rgba TICK_OFF{.r = 255, .g = 255, .b = 255, .a = 38};
constexpr ecs::Rgba BOOST_LOW{.r = 255, .g = 214, .b = 70, .a = 255};
constexpr ecs::Rgba BOOST_HIGH{.r = 255, .g = 120, .b = 24, .a = 255};

ecs::Rgba opaque(const Rgb color, const std::uint8_t alpha = 255) {
    return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = alpha};
}

ecs::Rgba withAlpha(ecs::Rgba color, const float alpha) {
    color.a = static_cast<std::uint8_t>(static_cast<float>(color.a) * std::clamp(alpha, 0.0F, 1.0F));
    return color;
}

ecs::Rgba blend(const ecs::Rgba from, const ecs::Rgba to, const float t) {
    const auto channel = [t](const std::uint8_t a, const std::uint8_t b) {
        return static_cast<std::uint8_t>(static_cast<float>(a) + ((static_cast<float>(b) - static_cast<float>(a)) * t));
    };
    return {
        .r = channel(from.r, to.r),
        .g = channel(from.g, to.g),
        .b = channel(from.b, to.b),
        .a = channel(from.a, to.a),
    };
}

/** @brief A clock value in seconds as "M:SS", with a "+" in overtime. */
std::string clockText(const int seconds, const bool overtime) {
    const int rest = seconds % 60;
    return (overtime ? "+" : "") + std::to_string(seconds / 60) + (rest < 10 ? ":0" : ":") + std::to_string(rest);
}

/** @brief Size factor of a text popping in, `elapsed` seconds after it appeared. */
float pop(const float elapsed) {
    const float t = std::clamp(elapsed / POP_TIME, 0.0F, 1.0F);
    const float ease = 1.0F - ((1.0F - t) * (1.0F - t) * (1.0F - t));
    return 1.0F + (POP_GROWTH * (1.0F - ease));
}

}  // namespace

void HudPlugin::build(engine::PluginBuilder& builder) {
    builder.require<AssetsPlugin>();
    builder.require<CameraPlugin>();
    builder.require<ecs::net::NetworkPlugin>();
}

void HudPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    const GameAssets& assets = registry.getResource<GameAssets>();
    _typeface = Typeface(engine.service<engine::IScreenDrawService>(), assets.font);
    _disc = assets.hudDisc;
    _tick = assets.hudTick;
    _slantLeft = assets.hudSlantLeft;
    _slantRight = assets.hudSlantRight;
    _band = assets.hudBand;

    _hud = registry.spawnEntity();
    // Hidden until a match is joined.
    registry.addComponent(_hud, game::MatchState{.phase = game::NO_MATCH});

    _packets =
        engine.eventBus().subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
            const auto message = protocol::decode(packet.data);
            const auto* status = message ? std::get_if<protocol::MatchStatus>(&*message) : nullptr;
            // A late status of a match just left must not show the HUD again in the menu.
            if (status == nullptr || !_inMatch) {
                return;
            }
            setState(engine, game::MatchState{
                                 .blueScore = status->blueScore,
                                 .orangeScore = status->orangeScore,
                                 .phase = static_cast<int>(status->phase),
                                 .countdown = status->countdown,
                                 .clock = status->clock,
                                 .overtime = status->overtime,
                             });
        });
    _joined = engine.eventBus().subscribe<MatchJoined>([this, &engine](const MatchJoined& /*event*/) {
        _inMatch = true;
        _goTime = 1.0e9F;
        setState(engine, game::MatchState{});
    });
    _left = engine.eventBus().subscribe<MatchLeft>([this, &engine](const MatchLeft& /*event*/) {
        _inMatch = false;
        setState(engine, game::MatchState{.phase = game::NO_MATCH});
    });
}

void HudPlugin::setState(engine::Engine& engine, const game::MatchState& state) {
    const game::MatchState previous = _state;
    if (state.countdown != previous.countdown) {
        _countdownTime = 0.0F;
    }
    if (state.phase == PHASE_PLAYING && previous.phase == PHASE_KICKOFF) {
        _goTime = 0.0F;
    }
    if (state.phase == PHASE_GOAL && previous.phase != PHASE_GOAL) {
        _goalTime = 0.0F;
        _blueScored = state.blueScore > previous.blueScore || state.orangeScore <= previous.orangeScore;
    }
    _state = state;
    auto& states = engine.registry().getComponents<game::MatchState>();
    if (states.contains(_hud)) {
        states.get(_hud) = state;
    }
}

void HudPlugin::lateUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    const float dt = time.deltaTime;
    _countdownTime += dt;
    _goTime += dt;
    _goalTime += dt;
    if (!_inMatch || _state.phase == game::NO_MATCH || _state.phase == PHASE_ENDED) {
        return;
    }
    const auto& window = engine.service<engine::IWindowService>();
    const auto width = static_cast<float>(window.width());
    const auto height = static_cast<float>(window.height());
    const float scale = std::clamp(height / DESIGN_HEIGHT, MIN_SCALE, MAX_SCALE);
    drawBoost(engine, width, height, scale, dt);
    drawAnnouncements(width, height, scale);
    drawScore(width, scale);
}

void HudPlugin::drawScore(const float width, const float scale) const {
    engine::IScreenDrawService* draw = _typeface.service();
    if (draw == nullptr) {
        return;
    }
    const float middle = width / 2.0F;
    const float top = SCORE_TOP * scale;
    const float panelHeight = SCORE_HEIGHT * scale;
    const float clockWidth = CLOCK_WIDTH * scale;
    const float panelWidth = PANEL_WIDTH * scale;
    const float shadow = SHADOW * scale;

    // Team panels on both sides of the clock, slanted on their outer edge.
    const engine::ScreenRect blue{
        .x = middle - (clockWidth / 2.0F) - panelWidth,
        .y = top,
        .width = panelWidth,
        .height = panelHeight,
    };
    const engine::ScreenRect orange{
        .x = middle + (clockWidth / 2.0F),
        .y = top,
        .width = panelWidth,
        .height = panelHeight,
    };
    draw->drawTexture(_slantLeft, WHOLE,
                      {.x = blue.x, .y = blue.y + shadow, .width = blue.width, .height = blue.height}, 0.0F,
                      SHADOW_COLOR);
    draw->drawTexture(_slantRight, WHOLE,
                      {.x = orange.x, .y = orange.y + shadow, .width = orange.width, .height = orange.height}, 0.0F,
                      SHADOW_COLOR);
    draw->drawTexture(_slantLeft, WHOLE, blue, 0.0F, opaque(BLUE_TEAM));
    draw->drawTexture(_slantRight, WHOLE, orange, 0.0F, opaque(ORANGE_TEAM));
    const engine::ScreenRect clock{
        .x = middle - (clockWidth / 2.0F),
        .y = top,
        .width = clockWidth,
        .height = panelHeight,
    };
    draw->fillRect({.x = clock.x, .y = clock.y + shadow, .width = clock.width, .height = clock.height}, SHADOW_COLOR);
    draw->fillRect(clock, CLOCK_PANEL);
    draw->fillRect(
        {.x = clock.x, .y = clock.y + clock.height - (3.0F * scale), .width = clock.width, .height = 3.0F * scale},
        CLOCK_EDGE);

    const float scoreCap = SCORE_CAP * scale;
    const float scoreTop = top + ((panelHeight - scoreCap) / 2.0F);
    const float slantShift = panelWidth * 0.06F;  // the slanted side eats into the panel: center on what is left
    for (const auto& [score, rect, shift] :
         {std::tuple{_state.blueScore, blue, slantShift}, std::tuple{_state.orangeScore, orange, -slantShift}}) {
        const std::string text = std::to_string(score);
        const float x = rect.x + (rect.width / 2.0F) + shift;
        _typeface.draw(text, x, scoreTop + (shadow * 0.6F), scoreCap, SHADOW_COLOR, Align::Center);
        _typeface.draw(text, x, scoreTop, scoreCap, WHITE, Align::Center);
    }

    const float clockCap = CLOCK_CAP * scale;
    _typeface.draw(clockText(_state.clock, _state.overtime), middle, top + ((panelHeight - clockCap) / 2.0F) - scale,
                   clockCap, _state.overtime ? GOLD : WHITE, Align::Center);
    if (_state.overtime) {
        const float labelCap = LABEL_CAP * scale;
        const float labelTop = top + panelHeight + (8.0F * scale);
        const float labelWidth = _typeface.width("PROLONGATION", labelCap) + (16.0F * scale);
        draw->fillRect(
            {
                .x = middle - (labelWidth / 2.0F),
                .y = labelTop - (6.0F * scale),
                .width = labelWidth,
                .height = labelCap + (12.0F * scale),
            },
            CLOCK_PANEL);
        _typeface.draw("PROLONGATION", middle, labelTop, labelCap, GOLD, Align::Center);
    }
}

void HudPlugin::drawAnnouncements(const float width, const float height, const float scale) const {
    engine::IScreenDrawService* draw = _typeface.service();
    if (draw == nullptr) {
        return;
    }
    const float middle = width / 2.0F;
    const auto bigText = [&](const std::string& text, const float centerY, const float cap, const ecs::Rgba color) {
        const float shadow = cap * 0.05F;
        _typeface.draw(text, middle + shadow, centerY - (cap / 2.0F) + shadow, cap,
                       withAlpha(SHADOW_COLOR, static_cast<float>(color.a) / 255.0F), Align::Center);
        _typeface.draw(text, middle, centerY - (cap / 2.0F), cap, color, Align::Center);
    };

    if (_state.phase == PHASE_KICKOFF && _state.countdown > 0) {
        const float cap = COUNTDOWN_CAP * scale * pop(_countdownTime);
        const float fade = std::clamp(_countdownTime / (POP_TIME * 0.6F), 0.0F, 1.0F);
        bigText(std::to_string(_state.countdown), height * COUNTDOWN_CENTER, cap, withAlpha(WHITE, fade));
    } else if (_state.phase == PHASE_PLAYING && _goTime < GO_TIME) {
        const float cap = COUNTDOWN_CAP * scale * pop(_goTime);
        const float fade = 1.0F - std::clamp((_goTime - (GO_TIME * 0.5F)) / (GO_TIME * 0.5F), 0.0F, 1.0F);
        bigText("GO!", height * COUNTDOWN_CENTER, cap, withAlpha(GOLD, fade));
    }

    if (_state.phase == PHASE_GOAL) {
        const Rgb team = _blueScored ? BLUE_TEAM : ORANGE_TEAM;
        const float grow = std::clamp(_goalTime / POP_TIME, 0.0F, 1.0F);
        const float centerY = height * BANNER_CENTER;
        const float bandWidth = BANNER_WIDTH * scale * (0.4F + (0.6F * grow));
        const float bandHeight = BANNER_HEIGHT * scale;
        draw->drawTexture(_band, WHOLE,
                          {
                              .x = middle - (bandWidth / 2.0F),
                              .y = centerY - (bandHeight / 2.0F),
                              .width = bandWidth,
                              .height = bandHeight,
                          },
                          0.0F, opaque(team, 215));
        bigText("GOAL!", centerY - (12.0F * scale), GOAL_CAP * scale * pop(_goalTime), WHITE);
        const float subCap = GOAL_SUB_CAP * scale;
        _typeface.draw(_blueScored ? "EQUIPE BLEUE" : "EQUIPE ORANGE", middle,
                       centerY + (GOAL_CAP * scale / 2.0F) + (2.0F * scale), subCap, withAlpha(WHITE, grow),
                       Align::Center);
    }
}

void HudPlugin::drawBoost(engine::Engine& engine, const float width, const float height, const float scale,
                          const float dt) {
    engine::IScreenDrawService* draw = _typeface.service();
    if (draw == nullptr) {
        return;
    }
    // The local car, found by the camera rig.
    ecs::Registry& registry = engine.registry();
    const auto& rigs = registry.getComponents<game::CameraRig>();
    const ecs::Entity camera = engine.getPlugin<CameraPlugin>().camera();
    if (!rigs.contains(camera) || rigs.get(camera).car == game::NO_ENTITY) {
        return;
    }
    const auto car = static_cast<ecs::Entity>(rigs.get(camera).car);
    const auto& statuses = registry.getComponents<game::CarStatus>();
    if (!statuses.contains(car)) {
        return;
    }
    const game::CarStatus& status = statuses.get(car);
    _shownBoost += (status.boost - _shownBoost) * std::min(1.0F, dt * BOOST_EASING);

    const float radius = BOOST_RADIUS * scale;
    const float centerX = width - radius - (BOOST_MARGIN * scale);
    const float centerY = height - radius - (BOOST_MARGIN * scale);
    const float discSize = radius * 2.5F;
    draw->drawTexture(
        _disc, WHOLE,
        {.x = centerX - (discSize / 2.0F), .y = centerY - (discSize / 2.0F), .width = discSize, .height = discSize},
        0.0F, WHITE);
    const float tickWidth = TICK_WIDTH * scale;
    const float tickLength = TICK_LENGTH * scale;
    const float ringRadius = radius - (tickLength / 2.0F);
    const float lit = std::clamp(_shownBoost, 0.0F, 100.0F) / 100.0F * static_cast<float>(BOOST_TICKS);
    for (int i = 0; i < BOOST_TICKS; ++i) {
        const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(BOOST_TICKS);
        const float angle = BOOST_START + (BOOST_SWEEP * t);
        const float x = centerX + (ringRadius * std::cos(angle));
        const float y = centerY + (ringRadius * std::sin(angle));
        const float on = std::clamp(lit - static_cast<float>(i), 0.0F, 1.0F);
        ecs::Rgba color = blend(TICK_OFF, blend(BOOST_LOW, BOOST_HIGH, t), on);
        if (status.boosting && on > 0.0F) {
            color = blend(color, WHITE, 0.35F);
        }
        // The tick texture stands upright: turn it so that it points away from the center.
        draw->drawTexture(
            _tick, WHOLE,
            {.x = x - (tickWidth / 2.0F), .y = y - (tickLength / 2.0F), .width = tickWidth, .height = tickLength},
            angle + (std::numbers::pi_v<float> / 2.0F), color);
    }
    const float cap = BOOST_CAP * scale;
    _typeface.draw(std::to_string(static_cast<int>(std::lround(_shownBoost))), centerX, centerY - (cap * 0.62F), cap,
                   WHITE, Align::Center);
    const float labelCap = LABEL_CAP * scale;
    _typeface.draw("BOOST", centerX, centerY + (cap * 0.55F), labelCap, DIM, Align::Center);
}

void HudPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
    if (_joined) {
        engine.eventBus().unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        engine.eventBus().unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
}

}  // namespace rlss::client
