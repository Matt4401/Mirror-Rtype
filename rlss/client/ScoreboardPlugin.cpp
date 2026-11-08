#include "ScoreboardPlugin.hpp"

#include <string>
#include <variant>

#include "Arena.hpp"
#include "CarViewPlugin.hpp"
#include "Shapes.hpp"
#include "components/Renderable.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::client {

namespace {

constexpr int GLYPH_COLUMNS = 3;
constexpr int GLYPH_ROWS = 5;

/** @brief Where and how big one display is drawn. */
struct Display {
    /** @brief Index read by the script: 0 blue, 1 orange, 2 countdown, 3 "GOAL". */
    int index = 0;
    /** @brief Number of characters. */
    int slots = 0;
    /** @brief Left edge of the first character, in world units. */
    float left = 0.0F;
    /** @brief Top edge, in world units. */
    float top = 0.0F;
    /** @brief Side of a square. */
    float cell = 0.0F;
    /** @brief Distance between two squares (square + gap). */
    float pitch = 0.0F;
    /** @brief Square color. */
    ecs::Rgba color;
};

constexpr float SCORE_CELL = 12.0F;
constexpr float SCORE_PITCH = 14.0F;
constexpr float SCORE_TOP = arena::TOP + 40.0F;
/** @brief Gap between the scores and the center line. */
constexpr float SCORE_GAP = 40.0F;
constexpr float BIG_CELL = 30.0F;
/** @brief The countdown ends this far above the arena center, where the ball waits for the kickoff. */
constexpr float COUNTDOWN_ABOVE_CENTER = 130.0F;
constexpr float BIG_PITCH = 34.0F;
constexpr float BANNER_CELL = 22.0F;
constexpr float BANNER_PITCH = 26.0F;
constexpr int LAYER_SCOREBOARD = 40;
constexpr ecs::Rgba COUNTDOWN_COLOR{.r = 255, .g = 255, .b = 255, .a = 235};
constexpr ecs::Rgba BANNER_COLOR{.r = 255, .g = 215, .b = 60, .a = 255};
constexpr ecs::Rgba DASH_COLOR{.r = 255, .g = 255, .b = 255, .a = 150};

/** @brief Width of a text of `slots` characters: glyphs plus one square of space between them. */
constexpr float textWidth(const int slots, const float pitch) {
    return static_cast<float>((slots * (GLYPH_COLUMNS + 1)) - 1) * pitch;
}

ecs::Rgba opaque(const Rgb color) { return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = 255}; }

void spawnDisplay(ecs::Registry& registry, const Display& display) {
    for (int slot = 0; slot < display.slots; ++slot) {
        for (int row = 0; row < GLYPH_ROWS; ++row) {
            for (int column = 0; column < GLYPH_COLUMNS; ++column) {
                const float x = display.left +
                                (static_cast<float>((slot * (GLYPH_COLUMNS + 1)) + column) * display.pitch) +
                                (display.pitch / 2.0F);
                const float y = display.top + (static_cast<float>(row) * display.pitch) + (display.pitch / 2.0F);
                const ecs::Entity cell = registry.spawnEntity();
                // Hidden (zero size) until the script lights it.
                registry.addComponent(cell, ecs::Transform{.x = x, .y = y, .scaleX = 0.0F, .scaleY = 0.0F});
                registry.addComponent(cell, makeRenderable(display.color, LAYER_SCOREBOARD));
                registry.addComponent(cell, ecs::Tag{.name = std::string(game::SCORE_CELL_TAG)});
                registry.addComponent(cell, game::ScoreCell{
                                                .display = display.index,
                                                .slot = slot,
                                                .row = row,
                                                .column = column,
                                                .size = display.cell,
                                            });
            }
        }
    }
}

}  // namespace

void ScoreboardPlugin::build(engine::PluginBuilder& builder) {
    builder.require<CarViewPlugin>();
    builder.require<ecs::net::NetworkPlugin>();
}

void ScoreboardPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    const float centerX = (arena::LEFT + arena::RIGHT) / 2.0F;
    const float centerY = (arena::TOP + arena::BOTTOM) / 2.0F;
    const float scoreWidth = textWidth(2, SCORE_PITCH);
    const float bigHeight = static_cast<float>(GLYPH_ROWS) * BIG_PITCH;
    const float bannerHeight = static_cast<float>(GLYPH_ROWS) * BANNER_PITCH;

    spawnDisplay(registry, {
                               .index = 0,
                               .slots = 2,
                               .left = centerX - SCORE_GAP - scoreWidth,
                               .top = SCORE_TOP,
                               .cell = SCORE_CELL,
                               .pitch = SCORE_PITCH,
                               .color = opaque(BLUE_TEAM),
                           });
    spawnDisplay(registry, {
                               .index = 1,
                               .slots = 2,
                               .left = centerX + SCORE_GAP,
                               .top = SCORE_TOP,
                               .cell = SCORE_CELL,
                               .pitch = SCORE_PITCH,
                               .color = opaque(ORANGE_TEAM),
                           });
    spawnDisplay(registry, {
                               .index = 2,
                               .slots = 1,
                               .left = centerX - (textWidth(1, BIG_PITCH) / 2.0F),
                               .top = centerY - COUNTDOWN_ABOVE_CENTER - bigHeight,
                               .cell = BIG_CELL,
                               .pitch = BIG_PITCH,
                               .color = COUNTDOWN_COLOR,
                           });
    spawnDisplay(registry, {
                               .index = 3,
                               .slots = 4,
                               .left = centerX - (textWidth(4, BANNER_PITCH) / 2.0F),
                               .top = centerY - bannerHeight,
                               .cell = BANNER_CELL,
                               .pitch = BANNER_PITCH,
                               .color = BANNER_COLOR,
                           });
    // Dash between the two scores.
    const float dashY = SCORE_TOP + (static_cast<float>(GLYPH_ROWS) * SCORE_PITCH / 2.0F);
    spawnRect(registry,
              {.left = centerX - 14.0F, .top = dashY - 3.0F, .right = centerX + 14.0F, .bottom = dashY + 3.0F},
              DASH_COLOR, LAYER_SCOREBOARD);

    _scoreboard = registry.spawnEntity();
    registry.addComponent(_scoreboard, game::MatchState{});
    registry.addComponent(_scoreboard, ecs::ScriptComponent{
                                           .moduleName = std::string(game::SCOREBOARD_SCRIPT_MODULE),
                                           .className = "ScoreboardView",
                                           .hasUpdate = true,
                                       });

    _packets =
        engine.eventBus().subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
            const auto message = protocol::decode(packet.data);
            const auto* status = message ? std::get_if<protocol::MatchStatus>(&*message) : nullptr;
            auto& states = engine.registry().getComponents<game::MatchState>();
            if (status == nullptr || !states.contains(_scoreboard)) {
                return;
            }
            states.get(_scoreboard) = game::MatchState{
                .blueScore = status->blueScore,
                .orangeScore = status->orangeScore,
                .phase = static_cast<int>(status->phase),
                .countdown = status->countdown,
            };
        });
}

void ScoreboardPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
}

}  // namespace rlss::client
