#include "AssetsPlugin.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "Logger.hpp"
#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/scripting/AssetScriptingPlugin.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "ecs/Registry.hpp"
#include "render/RenderPlugin.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "assets";
/** @brief More tiles than the arena will ever need: the lookup stops at the first missing one. */
constexpr int MAX_TILES = 16;

std::string tileAlias(const int row, const int column) {
    return "arena_" + std::to_string(row) + "_" + std::to_string(column);
}

}  // namespace

void AssetsPlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.require<assets::AssetScriptingPlugin>();
}

void AssetsPlugin::init(engine::Engine& engine) {
    auto& manager = engine.registry().getResource<assets::AssetManager>();
    manager.loadGroup(assets::AssetManager::GLOBAL_GROUP);
    const auto texture = [&manager](const std::string_view alias) {
        const ecs::TextureId id = manager.get<assets::Texture>(alias);
        if (id == ecs::NO_TEXTURE) {
            rtype::log::warning(LOG_TAG, "no texture '", alias, "' (run the client from the repository root)");
        }
        return id;
    };

    GameAssets loaded;
    loaded.stadium = texture("stadium");
    for (int row = 0; row < MAX_TILES && manager.get<assets::Texture>(tileAlias(row, 0)) != ecs::NO_TEXTURE; ++row) {
        for (int column = 0; column < MAX_TILES; ++column) {
            const ecs::TextureId tile = manager.get<assets::Texture>(tileAlias(row, column));
            if (tile == ecs::NO_TEXTURE) {
                break;
            }
            loaded.arena.push_back({.texture = tile, .row = row, .column = column});
        }
    }
    if (loaded.arena.empty()) {
        rtype::log::warning(LOG_TAG, "no arena texture");
    }
    loaded.carBody = texture("car_body");
    loaded.carDetails = texture("car_details");
    loaded.wheel = texture("wheel");
    loaded.flame = texture("boost_flame");
    loaded.glow = texture("glow");
    loaded.puff = texture("puff");
    loaded.ball = texture("ball");
    loaded.ballShade = texture("ball_shade");
    loaded.hudDisc = texture("hud_disc");
    loaded.hudTick = texture("hud_tick");
    loaded.hudSlantLeft = texture("hud_slant_left");
    loaded.hudSlantRight = texture("hud_slant_right");
    loaded.hudBand = texture("hud_band");
    loaded.explosionBlue = texture("goal_explosion_blue");
    loaded.explosionOrange = texture("goal_explosion_orange");
    loaded.shockwave = texture("shockwave");
    loaded.spark = texture("spark");
    loaded.font = manager.get<assets::Font>(FONT_ALIAS);
    if (loaded.font == engine::NO_FONT) {
        rtype::log::warning(LOG_TAG, "no font '", FONT_ALIAS, "', using the default font");
    }
    engine.registry().addResource(std::move(loaded));
}

}  // namespace rlss::client
