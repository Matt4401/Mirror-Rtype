/**
 * @file AssetsPlugin.hpp
 * @brief Loads the RLSS textures and font once, for the view plugins.
 */

#pragma once

#include <string_view>
#include <vector>

#include "components/Renderable.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"

namespace rlss::client {

/** @brief Wren module declaring the RLSS assets (`rlss/scripts/Assets.wren`), read from the content root. */
inline constexpr std::string_view ASSET_DECLARATIONS = "rlss/scripts/Assets";
/** @brief Alias of the font of the menus, the HUD and the name tags. */
inline constexpr std::string_view FONT_ALIAS = "rlss_font";

/** @brief One piece of the arena picture, too big for a single texture. */
struct ArenaTile {
    /** @brief Texture. */
    ecs::TextureId texture = ecs::NO_TEXTURE;
    /** @brief Row, from the top. */
    int row = 0;
    /** @brief Column, from the left. */
    int column = 0;
};

/**
 * @brief Every texture and the font of the game, an ECS resource (`registry.getResource<GameAssets>()`).
 *
 * A texture that cannot be loaded is `ecs::NO_TEXTURE` (logged): the arena and the stadium skip it, a car or ball part
 * using it is drawn as a small flat rectangle, the HUD skips it.
 */
struct GameAssets {
    /** @brief Stands and floodlights, behind the arena (parallax). */
    ecs::TextureId stadium = ecs::NO_TEXTURE;
    /** @brief Arena tiles, row by row. */
    std::vector<ArenaTile> arena;
    /** @brief Car body, greyscale: tinted with the player color. */
    ecs::TextureId carBody = ecs::NO_TEXTURE;
    /** @brief Car glass, trims and lights, drawn untinted over the body. */
    ecs::TextureId carDetails = ecs::NO_TEXTURE;
    /** @brief Wheel. */
    ecs::TextureId wheel = ecs::NO_TEXTURE;
    /** @brief Boost flame. */
    ecs::TextureId flame = ecs::NO_TEXTURE;
    /** @brief Soft round glow, white. */
    ecs::TextureId glow = ecs::NO_TEXTURE;
    /** @brief Boost trail particle, white. */
    ecs::TextureId puff = ecs::NO_TEXTURE;
    /** @brief Ball panels (turn with the ball). */
    ecs::TextureId ball = ecs::NO_TEXTURE;
    /** @brief Ball lighting (does not turn). */
    ecs::TextureId ballShade = ecs::NO_TEXTURE;
    /** @brief HUD: boost meter disc. */
    ecs::TextureId hudDisc = ecs::NO_TEXTURE;
    /** @brief HUD: one tick of the boost meter ring. */
    ecs::TextureId hudTick = ecs::NO_TEXTURE;
    /** @brief HUD: score panel slanted on its left side. */
    ecs::TextureId hudSlantLeft = ecs::NO_TEXTURE;
    /** @brief HUD: score panel slanted on its right side. */
    ecs::TextureId hudSlantRight = ecs::NO_TEXTURE;
    /** @brief HUD: soft horizontal band (banners). */
    ecs::TextureId hudBand = ecs::NO_TEXTURE;
    /** @brief Goal explosion of the blue team: a strip of `EXPLOSION_FRAMES` square frames. */
    ecs::TextureId explosionBlue = ecs::NO_TEXTURE;
    /** @brief Goal explosion of the orange team. */
    ecs::TextureId explosionOrange = ecs::NO_TEXTURE;
    /** @brief Shockwave ring, white. */
    ecs::TextureId shockwave = ecs::NO_TEXTURE;
    /** @brief Spark streak, white, its head on the right. */
    ecs::TextureId spark = ecs::NO_TEXTURE;
    /** @brief Font, or `engine::NO_FONT` (the backend's default font is used). */
    engine::FontId font = engine::NO_FONT;
};

/**
 * @brief Resolves `GameAssets` from the aliases declared by `rlss/scripts/Assets.wren` and adds it to the registry as
 * a resource. The `assets::AssetManager` owns the textures and the font.
 */
class AssetsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "AssetsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AssetsPlugin"; }

    /**
     * @brief Declares the dependencies on `render::RenderPlugin` (textures and font need its window) and
     * `assets::AssetScriptingPlugin` (the declarations).
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Loads the global asset group and resolves every texture and the font.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;
};

}  // namespace rlss::client
