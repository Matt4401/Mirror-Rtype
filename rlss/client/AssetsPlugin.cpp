#include "AssetsPlugin.hpp"

#include <filesystem>
#include <string>
#include <utility>

#include "Logger.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "ecs/Registry.hpp"
#include "render/RenderPlugin.hpp"
#include "render/texture/TextureStore.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "assets";
/** @brief More tiles than the arena will ever need: the loading stops at the first missing one. */
constexpr int MAX_TILES = 16;

ecs::TextureId load(render::TextureStore& textures, const std::string& file) {
    const std::string path = std::string(TEXTURE_DIRECTORY) + file;
    const ecs::TextureId id =
        std::filesystem::exists(path) ? textures.load(path, render::TextureFilter::Smooth) : ecs::NO_TEXTURE;
    if (id == ecs::NO_TEXTURE) {
        rtype::log::warning(LOG_TAG, "cannot load ", path, " (run the client from the repository root)");
    }
    return id;
}

std::string tileName(const int row, const int column) {
    return "arena_" + std::to_string(row) + "_" + std::to_string(column) + ".png";
}

}  // namespace

void AssetsPlugin::build(engine::PluginBuilder& builder) { builder.require<render::RenderPlugin>(); }

void AssetsPlugin::init(engine::Engine& engine) {
    render::TextureStore* textures = engine.getPlugin<render::RenderPlugin>().textures();
    GameAssets assets;
    if (textures != nullptr) {
        render::TextureStore& store = *textures;
        assets.stadium = load(store, "stadium.png");
        for (int row = 0; row < MAX_TILES && std::filesystem::exists(std::string(TEXTURE_DIRECTORY) + tileName(row, 0));
             ++row) {
            for (int column = 0; column < MAX_TILES; ++column) {
                if (!std::filesystem::exists(std::string(TEXTURE_DIRECTORY) + tileName(row, column))) {
                    break;
                }
                assets.arena.push_back({.texture = load(store, tileName(row, column)), .row = row, .column = column});
            }
        }
        if (assets.arena.empty()) {
            rtype::log::warning(LOG_TAG, "no arena texture in ", TEXTURE_DIRECTORY);
        }
        assets.carBody = load(store, "car_body.png");
        assets.carDetails = load(store, "car_details.png");
        assets.wheel = load(store, "wheel.png");
        assets.flame = load(store, "boost_flame.png");
        assets.glow = load(store, "glow.png");
        assets.puff = load(store, "puff.png");
        assets.ball = load(store, "ball.png");
        assets.ballShade = load(store, "ball_shade.png");
        assets.hudDisc = load(store, "hud_disc.png");
        assets.hudTick = load(store, "hud_tick.png");
        assets.hudSlantLeft = load(store, "hud_slant_left.png");
        assets.hudSlantRight = load(store, "hud_slant_right.png");
        assets.hudBand = load(store, "hud_band.png");
        assets.explosionBlue = load(store, "goal_explosion_blue.png");
        assets.explosionOrange = load(store, "goal_explosion_orange.png");
        assets.shockwave = load(store, "shockwave.png");
        assets.spark = load(store, "spark.png");
    }
    assets.font = engine.service<engine::IScreenDrawService>().loadFont(std::string(FONT_PATH));
    if (assets.font == engine::NO_FONT) {
        rtype::log::warning(LOG_TAG, "cannot load ", FONT_PATH, ", using the default font");
    }
    engine.registry().addResource(std::move(assets));
}

}  // namespace rlss::client
