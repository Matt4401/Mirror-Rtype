#include "ScoreboardPlugin.hpp"

#include <string>
#include <variant>

#include "Arena.hpp"
#include "CameraPlugin.hpp"
#include "CarViewPlugin.hpp"
#include "Shapes.hpp"
#include "components/Renderable.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IWindowService.hpp"
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

/** @brief Where and how big one display is drawn, on screen (pixels). */
struct Display {
    /** @brief Index read by the script: 0 blue, 1 orange, 2 countdown, 3 "GOAL". */
    int index = 0;
    /** @brief Number of characters. */
    int slots = 0;
    /** @brief Left edge of the first character. */
    float left = 0.0F;
    /** @brief Top edge. */
    float top = 0.0F;
    /** @brief Side of a square. */
    float cell = 0.0F;
    /** @brief Distance between two squares (square + gap). */
    float pitch = 0.0F;
    /** @brief Square color. */
    ecs::Rgba color;
};

constexpr float SCORE_CELL = 9.0F;
constexpr float SCORE_PITCH = 11.0F;
constexpr float SCORE_TOP = 14.0F;
/** @brief Gap between each score and the middle of the screen. */
constexpr float SCORE_GAP = 26.0F;
constexpr float DASH_WIDTH = 20.0F;
constexpr float DASH_HEIGHT = 5.0F;
constexpr float BIG_CELL = 24.0F;
constexpr float BIG_PITCH = 28.0F;
/** @brief Top of the countdown, as a share of the window height. */
constexpr float COUNTDOWN_TOP = 0.16F;
constexpr float BANNER_CELL = 18.0F;
constexpr float BANNER_PITCH = 22.0F;
constexpr float BANNER_TOP = 0.22F;
/** @brief Above everything drawn in the world. */
constexpr int LAYER_SCOREBOARD = 100;
constexpr ecs::Rgba COUNTDOWN_COLOR{.r = 255, .g = 255, .b = 255, .a = 235};
constexpr ecs::Rgba BANNER_COLOR{.r = 255, .g = 215, .b = 60, .a = 255};
constexpr ecs::Rgba DASH_COLOR{.r = 255, .g = 255, .b = 255, .a = 150};
/** @brief Display index of the squares always shown (the dash). */
constexpr int ALWAYS_SHOWN = 4;

/** @brief Width of a text of `slots` characters: glyphs plus one square of space between them. */
constexpr float textWidth(const int slots, const float pitch) {
    return static_cast<float>((slots * (GLYPH_COLUMNS + 1)) - 1) * pitch;
}

ecs::Rgba opaque(const Rgb color) { return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = 255}; }

/** @brief Spawns one square; the script places it in the world through the camera and lights it. */
void spawnCell(ecs::Registry& registry, const game::ScoreCell& cell, const ecs::Rgba color) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, ecs::Transform{.x = 0.0F, .y = 0.0F, .scaleX = 0.0F, .scaleY = 0.0F});
    registry.addComponent(entity, makeRenderable(color, LAYER_SCOREBOARD));
    registry.addComponent(entity, ecs::Tag{.name = std::string(game::SCORE_CELL_TAG)});
    registry.addComponent(entity, cell);
}

void spawnDisplay(ecs::Registry& registry, const Display& display) {
    for (int slot = 0; slot < display.slots; ++slot) {
        for (int row = 0; row < GLYPH_ROWS; ++row) {
            for (int column = 0; column < GLYPH_COLUMNS; ++column) {
                spawnCell(
                    registry,
                    game::ScoreCell{
                        .display = display.index,
                        .slot = slot,
                        .row = row,
                        .column = column,
                        .screenX = display.left +
                                   (static_cast<float>((slot * (GLYPH_COLUMNS + 1)) + column) * display.pitch) +
                                   (display.pitch / 2.0F),
                        .screenY = display.top + (static_cast<float>(row) * display.pitch) + (display.pitch / 2.0F),
                        .width = display.cell,
                        .height = display.cell,
                    },
                    display.color);
            }
        }
    }
}

}  // namespace

void ScoreboardPlugin::build(engine::PluginBuilder& builder) {
    builder.require<CarViewPlugin>();
    // The camera script must run before the scoreboard script, which places the squares through the camera.
    builder.require<CameraPlugin>();
    builder.require<ecs::net::NetworkPlugin>();
}

void ScoreboardPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    const auto& window = engine.service<engine::IWindowService>();
    const float middle = static_cast<float>(window.width()) / 2.0F;
    const auto height = static_cast<float>(window.height());
    const float scoreWidth = textWidth(2, SCORE_PITCH);

    spawnDisplay(registry, {
                               .index = 0,
                               .slots = 2,
                               .left = middle - SCORE_GAP - scoreWidth,
                               .top = SCORE_TOP,
                               .cell = SCORE_CELL,
                               .pitch = SCORE_PITCH,
                               .color = opaque(BLUE_TEAM),
                           });
    spawnDisplay(registry, {
                               .index = 1,
                               .slots = 2,
                               .left = middle + SCORE_GAP,
                               .top = SCORE_TOP,
                               .cell = SCORE_CELL,
                               .pitch = SCORE_PITCH,
                               .color = opaque(ORANGE_TEAM),
                           });
    spawnDisplay(registry, {
                               .index = 2,
                               .slots = 1,
                               .left = middle - (textWidth(1, BIG_PITCH) / 2.0F),
                               .top = height * COUNTDOWN_TOP,
                               .cell = BIG_CELL,
                               .pitch = BIG_PITCH,
                               .color = COUNTDOWN_COLOR,
                           });
    spawnDisplay(registry, {
                               .index = 3,
                               .slots = 4,
                               .left = middle - (textWidth(4, BANNER_PITCH) / 2.0F),
                               .top = height * BANNER_TOP,
                               .cell = BANNER_CELL,
                               .pitch = BANNER_PITCH,
                               .color = BANNER_COLOR,
                           });
    spawnCell(registry,
              game::ScoreCell{
                  .display = ALWAYS_SHOWN,
                  .screenX = middle,
                  .screenY = SCORE_TOP + (static_cast<float>(GLYPH_ROWS) * SCORE_PITCH / 2.0F),
                  .width = DASH_WIDTH,
                  .height = DASH_HEIGHT,
              },
              DASH_COLOR);

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
